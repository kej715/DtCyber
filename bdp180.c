/*--------------------------------------------------------------------------
**
**  Copyright (c) 2025, Kevin Jordan
**
**  Name: bdp180.c
**
**  Description:
**      This module contains functions supporting emulation of CYBER 180
**      BDP (Business Data Processing) instructions.
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License version 3 as
**  published by the Free Software Foundation.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License version 3 for more details.
**
**  You should have received a copy of the GNU General Public License
**  version 3 along with this program in file "license-gpl-3.0.txt".
**  If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.
**
**--------------------------------------------------------------------------
*/

#define DEBUG 0

/*
**  -------------
**  Include Files
**  -------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "const.h"
#include "proto.h"
#include "types.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/


/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#define IsLongZero(val) (((val)[0] | (val)[1] | (val)[2] | (val)[3]) == 0)

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void bdp180LongDiff(u64 *minend, u64 *subend, u64 *result, u64 *borrow);
static void bdp180LongDiv(u64 *dvdend, u64 *dvisor, u64 *quotient, u64 *remainder);
static void bdp180LongNegate(u64 *value);
static void bdp180LongSum(u64 *augend, u64 *addend, u64 *result, u64 *carry);

/*
**  ----------------
**  Public Variables
**  ----------------
*/

/*
**  -----------------
**  Private Variables
**  -----------------
*/

//
// Masks used in merging bytes into words
//
static u64 mergeMasks[9] =
    {
    0xffffffffffffffff,
    0x00ffffffffffffff,
    0x0000ffffffffffff,
    0x000000ffffffffff,
    0x00000000ffffffff,
    0x0000000000ffffff,
    0x000000000000ffff,
    0x00000000000000ff,
    0x0000000000000000
    };

//
// Maximum allowed length of each BDP operand type
//
static u16 maxBdpOpLengths[16] =
    {
    19,  // Type 0 : Packed Decimal No Sign
    19,  // Type 1 : Packed Decimal No Sign Leading Slack Digit
    19,  // Type 2 : Packed Decimal Signed
    19,  // Type 3 : Packed Decimal Signed Leading Slack Digit

    38,  // Type 4 : Unpacked Decimal Unsigned
    38,  // Type 5 : Unpacked Decimal Trailing Sign Combined Hollerith
    38,  // Type 6 : Unpacked Decimal Trailing Sign Separate
    38,  // Type 7 : Unpacked Decimal Leading Sign Combined Hollerith
    38,  // Type 8 : Unpacked Decimal Leading Sign Separate

    256, // Type 9 : Alphanumeric

    8,   // Type 10: Binary Unsigned
    8,   // Type 11: Binary Signed

    // Reserved types
    0, 0, 0, 0
    };

#if DEBUG
static FILE bdp180Log = NULL;
#endif

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Add two BDP operands
**
**  Parameters:     Name        Description.
**                  augend      pointer to augend
**                  addend      pointer to addend
**                  result      (out) pointer to result
**                  cond        (out) pointer to user condition on failure
**
**  Returns:        TRUE if success, FALSE if failure (e.g., arithmetic overflow)
**
**------------------------------------------------------------------------*/
bool bdp180Add(BdpOperand *augend, BdpOperand *addend, BdpOperand *result, UserCondition *cond)
    {
    u64 carry;
    bool isOk;
    u64 sum[4];

    isOk = TRUE;
    if (augend->sign == addend->sign)
        {
        result->sign = augend->sign;
        bdp180LongSum(augend->value, addend->value, sum, &carry);
        if (carry != 0 || sum[1] != 0 || sum[0] != 0)
            {
            *cond = UCR57; // Arithmetic overflow
            isOk  = FALSE;
            }
        }
    else
        {
        if (addend->sign)
            {
            bdp180LongDiff(augend->value, addend->value, sum, &carry);
            }
        else
            {
            bdp180LongDiff(addend->value, augend->value, sum, &carry);
            }
        result->sign = (sum[0] >> 63) != 0;
        if (result->sign)
            {
            bdp180LongNegate(sum);
            }
        }
    memcpy(result->value, sum, sizeof(sum));

    return isOk;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Add a digit to a BDP operand
**
**  Parameters:     Name         Description.
**                  operand      pointer to BDP operand
**                  digit        the digit to add
**
**------------------------------------------------------------------------*/
void bdp180AddDigit(BdpOperand *operand, u8 digit)
    {
    u64 carry;
    u64 t;

    t                  = operand->value[3];
    operand->value[3] += digit;
    carry              = operand->value[3] < t;

    t                  = operand->value[2];
    operand->value[2] += carry;
    carry              = operand->value[2] < t;

    t                  = operand->value[1];
    operand->value[1] += carry;
    carry              = operand->value[1] < t;

    operand->value[0] += carry;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Copy bytes from a buffer to a specified PVA
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  pva         the PVA to which to copy bytes
**                  count       the number of bytes to copy
**                  buffer      pointer to the source buffer
**
**  Returns:        TRUE if success, FALSE if failure (e.g., page fault)
**
**------------------------------------------------------------------------*/
bool bdp180CopyFromBuf(Cpu180Context *ctx, u64 pva, u16 count, u8 *buffer)
    {
    u8               *bp;
    MonitorCondition cond;
    u16              i;
    u64              mask;
    u16              n;
    u32              rmas[32];
    u8               shift;
    u64              word;
    u32              wordAddr;

    //
    //  Copy bytes from the buffer to the destination block. Optimize the copy by storing
    //  whole words in memory.
    //
    bp = buffer;
    if ((pva & Mask3) != 0 && count > 0)
        {
        //
        // Destination not word-aligned, so copy enough bytes to reach next word boundary
        //
        n     = 8 - (pva & Mask3);
        shift = (n - 1) << 3;
        if (n > count)
            {
            n = count;
            }
        if (cpu180TranslatePvaSequence(ctx, pva, 1, (u8)n, RingOf(pva), AccessModeWrite, rmas, &cond) == FALSE)
            {
            cpu180SetMonitorCondition(ctx, cond);
            return FALSE;
            }
        pva     += n;
        count   -= n;
        wordAddr = rmas[0] >> 3;
        word     = cpMem[wordAddr];
        while (n-- > 0)
            {
            mask   = ~((u64)0xffU << shift);
            word   = (word & mask) | ((u64)*bp++ << shift);
            shift -= 8;
            }
        cpMem[wordAddr] = word;
        }
    if (count > 0)
        {
        //
        // Copy up to eight bytes at a time, word-aligned
        //
        if (cpu180TranslatePvaSequence(ctx, pva, (count + 7) >> 3, 8, RingOf(pva), AccessModeWrite, rmas, &cond) == FALSE)
            {
            cpu180SetMonitorCondition(ctx, cond);
            return FALSE;
            }
        i = 0;
        while (count > 0) // copy a whole word at a time
            {
            n        = (count >= 8) ? 8 : count;
            count   -= n;
            wordAddr = rmas[i++] >> 3;
            word     = cpMem[wordAddr] & mergeMasks[n];
            shift    = 56;
            while (n-- > 0)
                {
                word  |= (u64)*bp++ << shift;
                shift -= 8;
                }
            cpMem[wordAddr] = word;
            }
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Copy bytes from a specified PVA to a buffer
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  pva         the PVA of the first byte
**                  count       the number of bytes to copy
**                  buffer      pointer to the destination buffer
**
**  Returns:        TRUE if success, FALSE if failure (e.g., page fault)
**
**------------------------------------------------------------------------*/
bool bdp180CopyToBuf(Cpu180Context *ctx, u64 pva, u16 count, u8 *buffer)
    {
    u8               *bp;
    MonitorCondition cond;
    u8               i;
    u16              n;
    u32              rmas[32];
    u8               shift;
    u64              word;

    //
    //  Copy bytes of the source block to the buffer. Optimize the copy by fetching
    //  whole words from memory.
    //
    bp = buffer;
    if ((pva & Mask3) != 0 && count > 0)
        {
        //
        // Source not word-aligned, so copy enough bytes to reach next word boundary
        //
        n     = 8 - (pva & Mask3);
        shift = (n - 1) << 3;
        if (n > count)
            {
            n = count;
            }
        if (cpu180TranslatePvaSequence(ctx, pva, 1, (u8)n, RingOf(pva), AccessModeRead, rmas, &cond) == FALSE)
            {
            cpu180SetMonitorCondition(ctx, cond);
            return FALSE;
            }
        pva   += n;
        count -= n;
        word   = cpMem[rmas[0] >> 3];
        while (n-- > 0)
            {
            *bp++  = (u8)(word >> shift);
            shift -= 8;
            }
        }
    //
    // Copy a whole word at a time
    //
    if (count > 0)
        {
        n = (count + 7) >> 3;
        if (cpu180TranslatePvaSequence(ctx, pva, n, 8, RingOf(pva), AccessModeRead, rmas, &cond) == FALSE)
            {
            cpu180SetMonitorCondition(ctx, cond);
            return FALSE;
            }
        i = 0;
        while (count > 0)
            {
            n      = (count >= 8) ? 8 : count;
            word   = cpMem[rmas[i++] >> 3];
            count -= n;
            shift  = 56;
            while (n-- > 0)
                {
                *bp++  = (u8)(word >> shift);
                shift -= 8;
                }
            }
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Decode a BDP operand
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  desc        pointer to BDP source descriptor
**                  operand     (out) decoded operand
**
**  Returns:        TRUE if successful
**
**                  MCR/UCR set if exception detected.
**
**------------------------------------------------------------------------*/
bool bdp180DecodeOperand(Cpu180Context *ctx, BdpDescriptor *desc, BdpOperand *operand)
    {
    u8  buffer[38];
    u8  d1;
    u8  d2;
    u8  i;
    u16 limit;
    u16 maxLength;

    static u64 signExt[9] =
        {
        0xffffffffffffffff,
        0xffffffffffffff00,
        0xffffffffffff0000,
        0xffffffffff000000,
        0xffffffff00000000,
        0xffffff0000000000,
        0xffff000000000000,
        0xff00000000000000,
        0x0000000000000000
        };

    maxLength = maxBdpOpLengths[desc->type];
    if (desc->length > maxLength || maxLength == 0)
        {
        cpu180SetMonitorCondition(ctx, MCR51); // Instruction specification error
        return FALSE;
        }
    if (bdp180CopyToBuf(ctx, desc->pva, desc->length, buffer) == FALSE)
        {
        return FALSE;
        }
    memset(operand, 0, sizeof(BdpOperand));
    switch (desc->type)
        {
    case 1:  // Packed Decimal No Sign Leading Slack Digit
        buffer[0] &= 0x0f;
        // fall through
    case 0:  // Packed Decimal No Sign
        for (i = 0; i < desc->length; i++)
            {
            d1 = buffer[i] >> 4;
            d2 = buffer[i] & Mask4;
            if (d1 > 9 || d2 > 9)
                {
                cpu180SetUserCondition(ctx, UCR63); // Invalid BDP data
                return FALSE;
                }
            bdp180Mul10(operand);
            bdp180AddDigit(operand, d1);
            bdp180Mul10(operand);
            bdp180AddDigit(operand, d2);
            }
        break;

    case 3:  // Packed Decimal Signed Leading Slack Digit
        buffer[0] &= 0x0f;
        // fall through
    case 2:  // Packed Decimal Signed
        if (desc->length > 0)
            {
            limit = desc->length - 1;
            d1 = buffer[limit] & Mask4;
            if (d1 == 0xd)
                {
                operand->sign = TRUE;
                }
            else if (d1 < 0xa)
                {
                cpu180SetUserCondition(ctx, UCR63); // Invalid BDP data
                return FALSE;
                }
            for (i = 0; i < limit; i++)
                {
                d1 = buffer[i] >> 4;
                d2 = buffer[i] & Mask4;
                if (d1 > 9 || d2 > 9)
                    {
                    cpu180SetUserCondition(ctx, UCR63); // Invalid BDP data
                    return FALSE;
                    }
                bdp180Mul10(operand);
                bdp180AddDigit(operand, d1);
                bdp180Mul10(operand);
                bdp180AddDigit(operand, d2);
                }
            d1 = buffer[limit] >> 4;
            if (d1 > 9)
                {
                cpu180SetUserCondition(ctx, UCR63); // Invalid BDP data
                return FALSE;
                }
            bdp180Mul10(operand);
            bdp180AddDigit(operand, d1);
            }
        break;

    case 4:  // Unpacked Decimal Unsigned
        for (i = 0; i < desc->length; i++)
            {
            d1 = buffer[i];
            if (d1 < 0x30 || d1 > 0x39)
                {
                cpu180SetUserCondition(ctx, UCR63); // Invalid BDP data
                return FALSE;
                }
            bdp180Mul10(operand);
            bdp180AddDigit(operand, d1 - 0x30);
            }
        break;

    case 5:  // Unpacked Decimal Trailing Sign Combined Hollerith
        if (desc->length > 0)
            {
            limit = desc->length - 1;
            for (i = 0; i < limit; i++)
                {
                d1 = buffer[i];
                if (d1 < 0x30 || d1 > 0x39)
                    {
                    cpu180SetUserCondition(ctx, UCR63); // Invalid BDP data
                    return FALSE;
                    }
                bdp180Mul10(operand);
                bdp180AddDigit(operand, d1 - 0x30);
                }
            d1 = buffer[limit];
            switch (d1)
                {
            case 0x31: // '1' - '9'
            case 0x32:
            case 0x33:
            case 0x34:
            case 0x35:
            case 0x36:
            case 0x37:
            case 0x38:
            case 0x39:
                d1 -= 0x30;
                break;
            case 0x41: // 'A' - 'I'
            case 0x42:
            case 0x43:
            case 0x44:
            case 0x45:
            case 0x46:
            case 0x47:
            case 0x48:
            case 0x49:
                d1 -= 0x40;
                break;
            case 0x4a: // 'J' - 'R'
            case 0x4b:
            case 0x4c:
            case 0x4d:
            case 0x4e:
            case 0x4f:
            case 0x50:
            case 0x51:
            case 0x52:
                d1           -= 0x49;
                operand->sign = TRUE;
                break;
            case 0x26: // '&'
            case 0x30: // '0'
            case 0x3c: // '<'
            case 0x7b: // '('
                d1 = 0;
                break;
            case 0x21: // '!'
            case 0x2d: // '-'
            case 0x7d: // ')'
                d1            = 0;
                operand->sign = TRUE;
                break;
            default:
                cpu180SetUserCondition(ctx, UCR63); // Invalid BDP data
                return FALSE;
                }
            bdp180Mul10(operand);
            bdp180AddDigit(operand, d1);
            }
        break;

    case 6:  // Unpacked Decimal Trailing Sign Separate
        if (desc->length > 0)
            {
            limit = desc->length - 1;
            for (i = 0; i < limit; i++)
                {
                d1 = buffer[i];
                if (d1 < 0x30 || d1 > 0x39)
                    {
                    cpu180SetUserCondition(ctx, UCR63); // Invalid BDP data
                    return FALSE;
                    }
                bdp180Mul10(operand);
                bdp180AddDigit(operand, d1 - 0x30);
                }
            d1 = buffer[limit];
            if (d1 == 0x2d) // '-'
                {
                operand->sign = TRUE;
                }
            else if (d1 != 0x2b) // '+'
                {
                cpu180SetUserCondition(ctx, UCR63); // Invalid BDP data
                return FALSE;
                }
            }
        break;

    case 7:  // Unpacked Decimal Leading Sign Combined Hollerith
        if (desc->length > 0)
            {
            d1 = buffer[0];
            switch (d1)
                {
            case 0x31: // '1' - '9'
            case 0x32:
            case 0x33:
            case 0x34:
            case 0x35:
            case 0x36:
            case 0x37:
            case 0x38:
            case 0x39:
                d1 -= 0x30;
                break;
            case 0x41: // 'A' - 'I'
            case 0x42:
            case 0x43:
            case 0x44:
            case 0x45:
            case 0x46:
            case 0x47:
            case 0x48:
            case 0x49:
                d1 -= 0x40;
                break;
            case 0x4a: // 'J' - 'R'
            case 0x4b:
            case 0x4c:
            case 0x4d:
            case 0x4e:
            case 0x4f:
            case 0x50:
            case 0x51:
            case 0x52:
                d1           -= 0x49;
                operand->sign = TRUE;
                break;
            case 0x26: // '&'
            case 0x30: // '0'
            case 0x3c: // '<'
            case 0x7b: // '('
                d1 = 0;
                break;
            case 0x21: // '!'
            case 0x2d: // '-'
            case 0x7d: // ')'
                d1            = 0;
                operand->sign = TRUE;
                break;
            default:
                cpu180SetUserCondition(ctx, UCR63); // Invalid BDP data
                return FALSE;
                }
            bdp180AddDigit(operand, d1);
            for (i = 1; i < desc->length; i++)
                {
                d1 = buffer[i];
                if (d1 < 0x30 || d1 > 0x39)
                    {
                    cpu180SetUserCondition(ctx, UCR63); // Invalid BDP data
                    return FALSE;
                    }
                bdp180Mul10(operand);
                bdp180AddDigit(operand, d1 - 0x30);
                }
            }
        break;

    case 8:  // Unpacked Decimal Leading Sign Separate
        if (desc->length > 0)
            {
            d1 = buffer[0];
            if (d1 == 0x2d) // '-'
                {
                operand->sign = TRUE;
                }
            else if (d1 != 0x2b) // '+'
                {
                cpu180SetUserCondition(ctx, UCR63); // Invalid BDP data
                return FALSE;
                }
            for (i = 1; i < desc->length; i++)
                {
                d1 = buffer[i];
                if (d1 < 0x30 || d1 > 0x39)
                    {
                    cpu180SetUserCondition(ctx, UCR63); // Invalid BDP data
                    return FALSE;
                    }
                bdp180Mul10(operand);
                bdp180AddDigit(operand, d1 - 0x30);
                }
            }
        break;

    case 10: // Binary Unsigned
        for (i = 0; i < desc->length; i++)
            {
            operand->value[3] = (operand->value[3] << 8) | buffer[i];
            }
        break;

    case 11: // Binary Signed
        if (desc->length > 0)
            {
            for (i = 0; i < desc->length; i++)
                {
                operand->value[3] = (operand->value[3] << 8) | buffer[i];
                }
            if (buffer[0] >= 0x80U)
                {
                operand->value[3] |= signExt[desc->length];
                operand->value[3]  = ~operand->value[3] + 1;
                operand->sign      = TRUE;
                }
            }
        break;

    default:
        cpu180SetMonitorCondition(ctx, MCR51); // Instruction specification error
        return FALSE;
        }

    operand->rawSign = operand->sign;
    if (operand->sign && IsLongZero(operand->value))
        {
        operand->sign = 0;
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Divide one BDP operand by another
**
**  Parameters:     Name        Description.
**                  dvdend      pointer to dividend
**                  dvisor      pointer to divisor
**                  result      (out) pointer to result
**                  cond        (out) pointer to user condition on failure
**
**  Returns:        TRUE if success, FALSE if failure (e.g., divide by zero)
**
**------------------------------------------------------------------------*/
bool bdp180Div(BdpOperand *dvdend, BdpOperand *dvisor, BdpOperand *result, UserCondition *cond)
    {
    u64  quotient[4];
    u64  remainder[4];

    if (IsLongZero(dvisor->value))
        {
        memset(result, 0, sizeof(BdpOperand));
        *cond = UCR55; // Divide fault
        return FALSE;
        }
    result->sign = IsLongZero(dvdend->value) ? 0 : dvdend->sign ^ dvisor->sign;
    bdp180LongDiv(dvdend->value, dvisor->value, quotient, remainder);
    memcpy(result->value, quotient, sizeof(quotient));
    if (quotient[1] != 0 || quotient[0] != 0)
        {
        *cond = UCR57; // Arithmetic overflow
        return FALSE;
        }
    else
        {
        return TRUE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Divide a BDP operand by 10
**
**  Parameters:     Name         Description.
**                  operand      pointer to the BDP operand
**                  remainder    (out) pointer to remainder
**
**------------------------------------------------------------------------*/
void bdp180Div10(BdpOperand *operand, u8 *remainder)
    {
    u64 dvisor[4];
    u64 quotient[4];
    u64 r[4];

    dvisor[0] = 0;
    dvisor[1] = 0;
    dvisor[2] = 0;
    dvisor[3] = 10;

    bdp180LongDiv(operand->value, dvisor, quotient, r);
    memcpy(operand->value, quotient, sizeof(quotient));
    *remainder = (u8)r[3];
    }

/*--------------------------------------------------------------------------
**  Purpose:        Encode a BDP operand
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  desc        pointer to BDP destination descriptor
**                  operand     pointer to operand to encode
**                  inhOnTrunc  TRUE to inhibit copying result to descriptor on truncation
**                  isTruncated (out) TRUE if result is truncated
**
**  Returns:        TRUE if successful (i.e., no MCR/UCR condition set)
**
**                  MCR/UCR set if exception detected.
**
**------------------------------------------------------------------------*/
bool bdp180EncodeOperand(Cpu180Context *ctx, BdpDescriptor *desc, BdpOperand *operand, bool inhOnTrunc, bool *isTruncated)
    {
    u8  buffer[256];
    u8  d1;
    u8  d2;
    u16 i;
    u16 maxLength;

    *isTruncated = FALSE;
    maxLength    = maxBdpOpLengths[desc->type];
    if (desc->length > maxLength)
        {
        cpu180SetMonitorCondition(ctx, MCR51); // Instruction specification error
        return FALSE;
        }
    switch (desc->type)
        {
    case 0:  // Packed Decimal No Sign
    case 1:  // Packed Decimal No Sign Leading Slack Digit
        i = desc->length;
        while (i > 0)
            {
            bdp180Div10(operand, &d1);
            bdp180Div10(operand, &d2);
            i -= 1;
            buffer[i] = (d2 << 4) | d1;
            }
        *isTruncated = IsLongZero(operand->value) == FALSE;
        if (desc->type == 1 && (desc->length < 1 || (buffer[0] & 0xf0) != 0))
            {
            *isTruncated = TRUE;
            buffer[0]   &= 0x0f;
            }
        break;
    case 2:  // Packed Decimal Signed
    case 3:  // Packed Decimal Signed Leading Slack Digit
        if (desc->length > 0)
            {
            i = desc->length - 1;
            bdp180Div10(operand, &d2);
            buffer[i] = (d2 << 4) | (operand->sign ? 0xd : 0xc);
            while (i > 0)
                {
                bdp180Div10(operand, &d1);
                bdp180Div10(operand, &d2);
                i -= 1;
                buffer[i] = (d2 << 4) | d1;
                }
            }
        *isTruncated = IsLongZero(operand->value) == FALSE;
        if (desc->type == 3 && (desc->length < 1 || (buffer[0] & 0xf0) != 0))
            {
            *isTruncated = TRUE;
            buffer[0]   &= 0x0f;
            }
        break;
    case 4:  // Unpacked Decimal Unsigned
        i = desc->length;
        while (i > 0)
            {
            bdp180Div10(operand, &d1);
            i -= 1;
            buffer[i] = d1 + 0x30;
            }
        *isTruncated = IsLongZero(operand->value) == FALSE;
        break;
    case 5:  // Unpacked Decimal Trailing Sign Combined Hollerith
        if (desc->length > 0)
            {
            bdp180Div10(operand, &d1);
            if (d1 == 0)
                {
                d1 = operand->sign ? 0x7d : 0x7b;
                }
            else if (operand->sign)
                {
                d1 += 0x49;
                }
            else
                {
                d1 += 0x40;
                }
            i         = desc->length - 1;
            buffer[i] = d1;
            while (i > 0)
                {
                bdp180Div10(operand, &d1);
                i -= 1;
                buffer[i] = d1 + 0x30;
                }
            *isTruncated = IsLongZero(operand->value) == FALSE;
            }
        else
            {
            *isTruncated = TRUE;
            }
        break;
    case 6:  // Unpacked Decimal Trailing Sign Separate
        if (desc->length > 0)
            {
            i         = desc->length - 1;
            buffer[i] = operand->sign ? 0x2d : 0x2b;
            while (i > 0)
                {
                bdp180Div10(operand, &d1);
                i -= 1;
                buffer[i] = d1 + 0x30;
                }
            *isTruncated = IsLongZero(operand->value) == FALSE;
            }
        else
            {
            *isTruncated = TRUE;
            }
        break;
    case 7:  // Unpacked Decimal Leading Sign Combined Hollerith
        if (desc->length > 0)
            {
            i = desc->length;
            while (i > 0)
                {
                bdp180Div10(operand, &d1);
                i -= 1;
                buffer[i] = d1 + 0x30;
                }
            d1 = buffer[0] - 0x30;
            if (d1 == 0)
                {
                d1 = operand->sign ? 0x7d : 0x7b;
                }
            else if (operand->sign)
                {
                d1 += 0x49;
                }
            else
                {
                d1 += 0x40;
                }
            buffer[0] = d1;
            *isTruncated = IsLongZero(operand->value) == FALSE;
            }
        else
            {
            *isTruncated = TRUE;
            }
        break;
    case 8:  // Unpacked Decimal Leading Sign Separate
        if (desc->length > 0)
            {
            i = desc->length;
            while (i > 0)
                {
                i -= 1;
                if (i == 0)
                    {
                    buffer[i] = operand->sign ? 0x2d : 0x2b;
                    *isTruncated = IsLongZero(operand->value) == FALSE;
                    }
                else
                    {
                    bdp180Div10(operand, &d1);
                    buffer[i] = d1 + 0x30;
                    }
                }
            }
        else
            {
            *isTruncated = TRUE;
            }
        break;
    case 11: // Binary Signed
        if (operand->sign)
            {
            bdp180LongNegate(operand->value);
            if (IsLongZero(operand->value) == FALSE)
                {
                operand->sign = !operand->sign;
                }
            }
        // fall through
    case 10: // Binary Unsigned
        i = desc->length;
        while (i > 0)
            {
            i                  -= 1;
            buffer[i]           = (u8)operand->value[3];
            operand->value[3] >>= 8;
            }
        *isTruncated = operand->value[3] != 0;
        break;
    default:
        cpu180SetMonitorCondition(ctx, MCR51); // Instruction specification error
        return FALSE;
        }

    if (*isTruncated == FALSE || inhOnTrunc == FALSE)
        {
        return bdp180CopyFromBuf(ctx, desc->pva, desc->length, buffer);
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Multiply two BDP operands
**
**  Parameters:     Name        Description.
**                  mltand      pointer to multiplicand
**                  mltier      pointer to multiplier
**                  result      (out) pointer to result
**                  cond        (out) pointer to user condition on failure
**
**  Returns:        TRUE if success, FALSE if failure (e.g., arithmetic overflow)
**
**------------------------------------------------------------------------*/
bool bdp180Mul(BdpOperand *mltand, BdpOperand *mltier, BdpOperand *result, UserCondition *cond)
    {
    u64  carry;
    u64  product[4];
    u64  t;

    result->sign = (IsLongZero(mltier->value) || IsLongZero(mltand->value)) ? 0 : mltand->sign ^ mltier->sign;
    memset(product, 0, sizeof(product));
    while (IsLongZero(mltier->value) == FALSE)
        {
        //
        //  If the LSB of multiplier is 1, add multiplicand to product
        //
        if ((mltier->value[3] & 1) != 0)
            {
            t           = product[3];
            product[3] += mltand->value[3];
            carry       = product[3] < t;
            t           = product[2];
            product[2] += mltand->value[2] + carry;
            carry       = product[2] < t;
            t           = product[1];
            product[1] += mltand->value[1] + carry;
            carry       = product[1] < t;
            product[0] += mltand->value[0] + carry;
            }
        //
        //  Left shift multiplicand (multiply by 2)
        //
        mltand->value[0]   = (mltand->value[0] << 1) | (mltand->value[1] >> 63);
        mltand->value[1]   = (mltand->value[1] << 1) | (mltand->value[2] >> 63);
        mltand->value[2]   = (mltand->value[2] << 1) | (mltand->value[3] >> 63);
        mltand->value[3] <<= 1;
        //
        //  Right shift multiplier (divide by 2)
        //
        mltier->value[3]   = (mltier->value[3] >> 1) | ((mltier->value[2] & 1) << 63);
        mltier->value[2]   = (mltier->value[2] >> 1) | ((mltier->value[1] & 1) << 63);
        mltier->value[1]   = (mltier->value[1] >> 1) | ((mltier->value[0] & 1) << 63);
        mltier->value[0] >>= 1;
        }
    memcpy(result->value, product, sizeof(product));
    if (product[1] != 0 || product[0] != 0)
        {
        *cond = UCR57; // Arithmetic overflow
        return FALSE;
        }
    else
        {
        return TRUE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Multiply a BDP operand by 10
**
**  Parameters:     Name         Description.
**                  operand      pointer to BDP operand
**
**------------------------------------------------------------------------*/
void bdp180Mul10(BdpOperand *operand)
    {
    u64 carry;
    u64 f2[4];
    u64 f8[4];

    //
    // x * 10 = (x << 1) + (x << 3)
    //
    f2[0] = (operand->value[0] << 1) | (operand->value[1] >> 63);
    f2[1] = (operand->value[1] << 1) | (operand->value[2] >> 63);
    f2[2] = (operand->value[2] << 1) | (operand->value[3] >> 63);
    f2[3] = operand->value[3] << 1;

    f8[0] = (f2[0] << 1) | (f2[1] >> 63);
    f8[1] = (f2[1] << 1) | (f2[2] >> 63);
    f8[2] = (f2[2] << 1) | (f2[3] >> 63);
    f8[3] = f2[3] << 1;

    f8[0] = (f8[0] << 1) | (f8[1] >> 63);
    f8[1] = (f8[1] << 1) | (f8[2] >> 63);
    f8[2] = (f8[2] << 1) | (f8[3] >> 63);
    f8[3] = f8[3] << 1;

    operand->value[3] = f8[3] + f2[3];
    carry             = operand->value[3] < f8[3];
    operand->value[2] = f8[2] + f2[2] + carry;
    carry             = operand->value[2] < f8[2];
    operand->value[1] = f8[1] + f2[1] + carry;
    carry             = operand->value[1] < f8[1];
    operand->value[0] = f8[0] + f2[0] + carry;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Subtract two BDP operands
**
**  Parameters:     Name        Description.
**                  minend      pointer to minuend
**                  subend      pointer to subtrahend
**                  result      (out) pointer to result
**                  cond        (out) pointer to user condition on failure
**
**  Returns:        TRUE if success, FALSE if failure (e.g., arithmetic overflow)
**
**------------------------------------------------------------------------*/
bool bdp180Sub(BdpOperand *minend, BdpOperand *subend, BdpOperand *result, UserCondition *cond)
    {
    BdpOperand t;

    t      = *subend;
    t.sign = !t.sign;
    return bdp180Add(minend, &t, result, cond);
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Calculate the difference between two 256-bit, unsigned integers
**
**  Parameters:     Name         Description.
**                  minend       pointer to 256-bit minuend
**                  subend       pointer to 256-bit subtrahend
**                  result       (out) 256-bit difference
**                  borrow       (out) 1 if minuend < subtrahend, 0 otherwise
**
**------------------------------------------------------------------------*/
static void bdp180LongDiff(u64 *minend, u64 *subend, u64 *result, u64 *borrow)
    {
    u64 diff;

    diff      = minend[3] - subend[3];
    *borrow   = diff > minend[3];
    result[3] = diff;

    diff      = (minend[2] - subend[2]) - *borrow;
    *borrow   = diff > minend[2];
    result[2] = diff;

    diff      = (minend[1] - subend[1]) - *borrow;
    *borrow   = diff > minend[1];
    result[1] = diff;

    diff      = (minend[0] - subend[0]) - *borrow;
    *borrow   = diff > minend[0];
    result[0] = diff;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Calculate the quotient and remainder of a 256-bit, unsigned
**                  dividend and divisor
**
**  Parameters:     Name         Description.
**                  dvdend       pointer to 256-bit dividend
**                  dvisor       pointer to 256-bit divisor
**                  quotient     (out) 256-bit quotient
**                  remainder    (out) 256-bit remainder
**
**------------------------------------------------------------------------*/
static void bdp180LongDiv(u64 *dvdend, u64 *dvisor, u64 *quotient, u64 *remainder)
    {
    u64 borrow;
    int i;
    u64 t[4];

    memset(quotient, 0, sizeof(u64) * 4);

    if (IsLongZero(dvdend))
        {
        memset(remainder, 0, sizeof(u64) * 4);
        return;
        }

    bdp180LongDiff(dvdend, dvisor, t, &borrow);
    if (borrow != 0)
        {
        memcpy(remainder, dvdend, sizeof(u64) * 4);
        return;
        }

    // Align the divisor to the most significant bit of the dividend
    i = 1;
    for (;;)
        {
        bdp180LongDiff(dvdend, dvisor, t, &borrow);
        if (borrow || (dvisor[0] & 0x8000000000000000) != 0) break;
        dvisor[0]   = (dvisor[0] << 1) | (dvisor[1] >> 63);
        dvisor[1]   = (dvisor[1] << 1) | (dvisor[2] >> 63);
        dvisor[2]   = (dvisor[2] << 1) | (dvisor[3] >> 63);
        dvisor[3] <<= 1;
        i          += 1;
        }

    // Perform the repeated shift and subtract
    while (i-- > 0)
        {
        quotient[0]   = (quotient[0] << 1) | (quotient[1] >> 63);
        quotient[1]   = (quotient[1] << 1) | (quotient[2] >> 63);
        quotient[2]   = (quotient[2] << 1) | (quotient[3] >> 63);
        quotient[3] <<= 1;
        bdp180LongDiff(dvdend, dvisor, t, &borrow);
        if (borrow == 0)
            {
            memcpy(dvdend, t, sizeof(t));
            quotient[3] |= 1;
            }
        dvisor[3]   = (dvisor[3] >> 1) | ((dvisor[2] & 1) << 63);
        dvisor[2]   = (dvisor[2] >> 1) | ((dvisor[1] & 1) << 63);
        dvisor[1]   = (dvisor[1] >> 1) | ((dvisor[0] & 1) << 63);
        dvisor[0] >>= 1;
        }

    memcpy(remainder, dvdend, sizeof(u64) * 4);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Negate a 256-bit integer value
**
**  Parameters:     Name         Description.
**                  value        pointer to the value
**
**------------------------------------------------------------------------*/
static void bdp180LongNegate(u64 *value)
    {
    u64 carry;
    u64 t;

    t        = ~value[3];
    value[3] = t + 1;
    carry    = value[3] < t;
    t        = ~value[2];
    value[2] = t + carry;
    carry    = value[2] < t;
    t        = ~value[1];
    value[1] = t + carry;
    carry    = value[1] < t;
    value[0] = ~value[0] + carry;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Calculate the sum of two 256-bit, unsigned integers
**
**  Parameters:     Name         Description.
**                  augend       pointer to 256-bit augend
**                  addend       pointer to 256-bit addend
**                  result       (out) 256-bit sum
**                  carry        (out) 1 if carry (i.e., arithmetic overflow)
**
**------------------------------------------------------------------------*/
static void bdp180LongSum(u64 *augend, u64 *addend, u64 *result, u64 *carry)
    {
    result[3] = augend[3] + addend[3];
    *carry    = result[3] < augend[3];

    result[2] = augend[2] + addend[2] + *carry;
    *carry    = result[2] < augend[2];

    result[1] = augend[1] + addend[1] + *carry;
    *carry    = result[1] < augend[1];

    result[0] = augend[0] + addend[0] + *carry;
    *carry    = result[0] < augend[0];
    }

/*---------------------------  End Of File  ------------------------------*/
