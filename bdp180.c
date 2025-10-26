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
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

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
static void bdp180AddDigit(BdpOperand *operand, u8 digit);
static void bdp180Div10(BdpOperand *operand, u8 *remainder);
static void bdp180Mul10(BdpOperand *operand);
static void bdp180Negate(BdpOperand *operand);

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
#if defined(_WIN32)
    // TODO: implement this for Windows
    return TRUE;
#else
    u128 a1;
    u128 a2;
    bool isOk;
    u128 sum;

    a1   = ((u128)augend->value[0] << 64) | (u128)augend->value[1];
    a2   = ((u128)addend->value[0] << 64) | (u128)addend->value[1];
    isOk = TRUE;
    if (augend->sign == addend->sign)
        {
        result->sign = augend->sign;
        sum          = a1 + a2;
        if (sum < a1 || sum < a2)
            {
            *cond = UCR57; // Arithmetic overflow
            isOk  = FALSE;
            }
        }
    else
        {
        if (addend->sign)
            {
            sum = a1 - a2;
            }
        else
            {
            sum = a2 - a1;
            }
        result->sign = (sum >> 127) != 0;
        if (result->sign)
            {
            sum = ~sum + 1;
            }
        }
    result->value[0] = (u64)(sum >> 64);
    result->value[1] = (u64)sum;
    return isOk;
#endif
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
    u8  *bp;
    u16 i;
    u16 n;
    u64 word;

    //
    //  Copy bytes from the buffer to the destination block. Optimize the copy by storing
    //  whole words in memory.
    //
    bp = buffer;
    if ((pva & Mask3) != 0) // destination not word-aligned, so copy enough bytes to word-align it
        {
        n = 8 - (pva & Mask3);
        if (n > count)
            {
            n = count;
            }
        word = 0;
        for (i = 0; i < n; i++)
            {
            word = (word << 8) | *bp++;
            }
        if (cpu180PutBytes(ctx, pva, RingOf(pva), word, n) == FALSE)
            {
            return FALSE;
            }
        pva   += n;
        count -= n;
        }
    while (count > 0) // copy a whole word at a time
        {
        n = (count >= 8) ? 8 : count;
        word = 0;
        for (i = 0; i < n; i++)
            {
            word = (word << 8) | *bp++;
            }
        if (cpu180PutBytes(ctx, pva, RingOf(pva), word, n) == FALSE)
            {
            return FALSE;
            }
        pva   += n;
        count -= n;
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
    u8  *bp;
    u16 n;
    u8  shift;
    u64 word;

    //
    //  Copy bytes of the source block to the buffer. Optimize the copy by fetching
    //  whole words from memory.
    //
    bp  = buffer;
    if ((pva & Mask3) != 0) // source not word-aligned, so copy enough bytes to word-align it
        {
        n = 8 - (pva & Mask3);
        if (n > count)
            {
            n = count;
            }
        if (cpu180GetBytes(ctx, pva, n, RingOf(pva), AccessModeRead, &word) == FALSE)
            {
            return FALSE;
            }
        pva   += n;
        count -= n;
        shift  = (n - 1) << 3;
        while (n-- > 0)
            {
            *bp++  = (u8)((word >> shift) & 0xff);
            shift -= 8;
            }
        }
    while (count > 0) // copy a whole word at a time
        {
        n = (count >= 8) ? 8 : count;
        if (cpu180GetBytes(ctx, pva, n, RingOf(pva), AccessModeRead, &word) == FALSE)
            {
            return FALSE;
            }
        pva   += n;
        count -= n;
        shift  = (n - 1) << 3;
        while (n-- > 0)
            {
            *bp++  = (u8)((word >> shift) & 0xff);
            shift -= 8;
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
            operand->value[1] = (operand->value[1] << 8) | buffer[i];
            }
        break;

    case 11: // Binary Signed
        for (i = 0; i < desc->length; i++)
            {
            operand->value[1] = (operand->value[1] << 8) | buffer[i];
            }
        if (buffer[0] >= 0x80)
            {
            operand->value[1] |= signExt[desc->length];
            operand->value[1]  = ~operand->value[1] + 1;
            operand->sign      = TRUE;
            }
        break;

    default:
        cpu180SetMonitorCondition(ctx, MCR51); // Instruction specification error
        return FALSE;
        }

    if (operand->sign && operand->value[1] == 0 && operand->value[0] == 0)
        {
        operand->sign = 0;
        }

    return TRUE;
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
    u8  buffer[512];
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
        *isTruncated = operand->value[1] != 0 || operand->value[0] != 0;
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
        *isTruncated = operand->value[1] != 0 || operand->value[0] != 0;
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
        *isTruncated = operand->value[1] != 0 || operand->value[0] != 0;
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
            *isTruncated = operand->value[1] != 0 || operand->value[0] != 0;
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
            *isTruncated = operand->value[1] != 0 || operand->value[0] != 0;
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
            if (operand->value[1] == 0 && operand->value[0] == 0)
                {
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
                }
            else
                {
                *isTruncated = TRUE;
                }
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
                    if (operand->value[1] == 0 && operand->value[0] == 0)
                        {
                        buffer[i] = operand->sign ? 0x2d : 0x2b;
                        }
                    else
                        {
                        bdp180Div10(operand, &d1);
                        buffer[i] = d1 + 0x30;
                        *isTruncated = TRUE;
                        }
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
    case 10: // Binary Unsigned
    case 11: // Binary Signed
        if (operand->sign)
            {
            bdp180Negate(operand);
            }
        i = desc->length;
        while (i > 0)
            {
            i                  -= 1;
            buffer[i]           = (u8)operand->value[1];
            operand->value[1] >>= 8;
            }
        *isTruncated = operand->value[1] != 0;
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
    u64  m256[4];
    u64  p256[4];
    u64  t;

    m256[0]          = 0;
    m256[1]          = 0;
    m256[2]          = mltand->value[0];
    m256[3]          = mltand->value[1];
    result->sign     = ((mltier->value[0] | mltier->value[1]) == 0 || (mltand->value[0] | mltand->value[1]) == 0) ? 0 : mltand->sign ^ mltier->sign;
    memset(p256, 0, sizeof(p256));
    while (mltier->value[0] != 0 || mltier->value[1] != 0)
        {
        //
        //  If the LSB of multiplier is 1, add multiplicand to product
        //
        if ((mltier->value[1] & 1) != 0)
            {
            t        = p256[3];
            p256[3] += m256[3];
            carry    = p256[3] < t;
            t        = p256[2];
            p256[2] += m256[2] + carry;
            carry    = p256[2] < t;
            t        = p256[1];
            p256[1] += m256[1] + carry;
            carry    = p256[1] < t;
            p256[0] += m256[0] + carry;
            }
        //
        //  Left shift multiplicand (multiply by 2)
        //
        m256[0]   = (m256[0] << 1) | (m256[1] >> 63);
        m256[1]   = (m256[1] << 1) | (m256[2] >> 63);
        m256[2]   = (m256[2] << 1) | (m256[3] >> 63);
        m256[3] <<= 1;
        //
        //  Right shift multiplier (divide by 2)
        //
        mltier->value[1]  = (mltier->value[1] >> 1) | ((mltier->value[0] & 1) << 63);
        mltier->value[0] >>= 1;
        }
    result->value[0] = p256[2];
    result->value[1] = p256[3];
    if (p256[1] != 0 || p256[0] != 0)
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
**  Purpose:        Add a digit to a BDP operand
**
**  Parameters:     Name         Description.
**                  operand      pointer to BDP operand
**                  digit        the digit to add
**
**------------------------------------------------------------------------*/
static void bdp180AddDigit(BdpOperand *operand, u8 digit)
    {
    operand->value[1] += digit;
    if (operand->value[1] < digit)
        {
        operand->value[0] += 1;
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
static void bdp180Div10(BdpOperand *operand, u8 *remainder)
    {
#if defined(_WIN32)
    u8  divisor;
    u8  i;
    u64 q128[2];

    divisor      = 10;
    memset(q128, 0, sizeof(q128));

    for (i = 0; i < 4; i++)
        {
        //
        //  Shift divisor right
        //
        divisor >>= 1;
        //
        //  Shift quotient left to make space for new bit
        //
        q128[0]   = (q128[0] << 1) | (q128[1] >> 63);
        q128[1] <<= 1;
        //
        //  Subtract divisor from remainder, if possible
        //
        if (operand->value[0] > 0 || operand->value[1] >= divisor)
            {
            if (operand->value[1] < divisor)
                {
                operand->value[1] -= divisor;
                operand->value[0] -= 1;
                }
            else
                {
                operand->value[1] -= divisor;
                }
            q128[1] |= 1;
            }
        }
    *remainder        = (u8)operand->value[1];
    operand->value[0] = q128[0];
    operand->value[1] = q128[1];
#else
    u128 d128;
    u128 q128;

    d128              = ((u128)operand->value[0] << 64) | (u128)operand->value[1];
    q128              = d128 / (u128)10;
    operand->value[0] = (u64)(q128 >> 64);
    operand->value[1] = (u64)q128;
    *remainder        = (u8)(d128 % 10);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Multiply a BDP operand by 10
**
**  Parameters:     Name         Description.
**                  operand      pointer to BDP operand
**
**------------------------------------------------------------------------*/
static void bdp180Mul10(BdpOperand *operand)
    {
#if defined(_WIN32)
    u64 f2[2];
    u64 f8[2];

    //
    // x * 10 = (x << 1) + (x << 3)
    //
    f2[0] = (operand->value[0] << 1) | (operand->value[1] >> 63);
    f2[1] = operand->value[1] << 1;

    f8[0] = (f2[0] << 1) | (f2[1] >> 63);
    f8[1] = f2[1] << 1;

    f8[0] = (f8[0] << 1) | (f8[1] >> 63);
    f8[1] = f8[1] << 1;

    operand->value[1] = f8[1] + f2[1];
    operand->value[0] = f8[0] + f2[0];
    if (operand->value[1] < f8[1] || operand->value[1] < f2[1])
        {
        operand->value[0] += 1;
        }
#else
    u128 p128;

    p128 = (((u128)operand->value[0] << 64) | (u64)operand->value[1]) * (u128)10;
    operand->value[0] = (u64)(p128 >> 64);
    operand->value[1] = (u64)p128;
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Negate a BDP operand
**
**  Parameters:     Name         Description.
**                  operand      pointer to BDP operand
**
**------------------------------------------------------------------------*/
static void bdp180Negate(BdpOperand *operand)
    {
    u64 v;

    operand->value[0] = ~operand->value[0];
    v = ~operand->value[1] + 1;
    if (v < operand->value[1])
        {
        operand->value[0] += 1;
        }
    operand->value[1] = v;
    operand->sign = !operand->sign;
    }

/*---------------------------  End Of File  ------------------------------*/
