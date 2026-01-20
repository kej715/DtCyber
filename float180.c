/*--------------------------------------------------------------------------
**
**  Copyright (c) 2025, Kevin Jordan
**
**  Name: float180.c
**
**  Description:
**      Perform emulation of CDC CYBER 180 floating point operations.
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
#if defined(_WIN32)
#undef INFINITE
#endif

#define CoefficientOf(fval) ((fval) & Mask48)
#define ExponentOf(fval) (((fval) >> 48) & 0x7fff)
#define SignOf(fval) (((fval) >> 63) & 1)

#define BIAS           0x4000
#define INDEFINITE     0x7000000000000000
#define INFINITE       0x5000000000000000
#define NEG_INDEFINITE 0xF000000000000000
#define NEG_INFINITE   0xD000000000000000

#define FP_ONE         0x4001800000000000
#define FP__NEG_ONE    0xc001800000000000

#define IsIndefinite(exponent) ((exponent) >= 0x7000)
#define IsInfinite(exponent) ((exponent) >= 0x5000 && (exponent) < 0x7000)
#define IsStandard(exponent) ((exponent) >= 0x3000 && (exponent) < 0x5000)
#define IsZ1(exponent) ((exponent) < 0x1000)
#define IsZ2(exponent) ((exponent) < 0x3000 && (exponent) >= 0x1000)
#define IsZ3(exponent, coefficient) ((coefficient) == 0 && IsStandard(exponent))

#define IsUcTrapEnabled(ctx, cond) ((ctx->regUmr & ucrMasks[cond]) != 0 && (ctx->regFlags & 0x3) == 2)
#define IsUmrBitSet(ctx, cond) ((ctx->regUmr & ucrMasks[cond]) != 0)
#define UcrBitMask(cond) (ucrMasks[cond])

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/

/*
**  Classes of floating point values
*/
typedef enum
    {
    FloatClass_Z1Z2 = 0,
    FloatClass_Z3,
    FloatClass_N,
    FloatClass_Indefinite,
    FloatClass_Infinite
    } FloatClass;

//
//  Combinations of floating point classes
//
#define Z1Z2xZ1Z2    0
#define Z1Z2xZ3      1
#define Z1Z2xN       2
#define Z1Z2xINDEF   3
#define Z1Z2xINF     4

#define Z3xZ1Z2      5 
#define Z3xZ3        6
#define Z3xN         7
#define Z3xINDEF     8
#define Z3xINF       9

#define NxZ1Z2      10
#define NxZ3        11
#define NxN         12
#define NxINDEF     13
#define NxINF       14

#define INDEFxZ1Z2  15
#define INDEFxZ3    16
#define INDEFxN     17
#define INDEFxINDEF 18
#define INDEFxINF   19

#define INFxZ1Z2    20
#define INFxZ3      21
#define INFxN       22
#define INFxINDEF   23
#define INFxINF     24

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FloatClass float180DoubleClassOf(Cpu180Double *value);
static FloatClass float180FloatClassOf(u64 value);
static void float180LongDiv(Cpu180Double *dvdend, Cpu180Double *dvisor, Cpu180Double *quotient);
static void float180LongMul(Cpu180Double *mltand, Cpu180Double *mltier, Cpu180Double *hiProd, Cpu180Double *loProd);
static void float180NormalizeDouble(u16 *exponent, Cpu180Double *coefficient);
static void float180NormalizeFloat(u16 *exponent, u64 *coefficient);

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
//  Masks for right-most bits in double precision coefficients,
//  indexed by shift counts
//
static u64 coeffMasks[48] =
    {
    0x000000000000,
    0x000000000001,
    0x000000000003,
    0x000000000007,
    0x00000000000f,
    0x00000000001f,
    0x00000000003f,
    0x00000000007f,
    0x0000000000ff,
    0x0000000001ff,
    0x0000000003ff,
    0x0000000007ff,
    0x000000000fff,
    0x000000001fff,
    0x000000003fff,
    0x000000007fff,
    0x00000000ffff,
    0x00000001ffff,
    0x00000003ffff,
    0x00000007ffff,
    0x0000000fffff,
    0x0000001fffff,
    0x0000003fffff,
    0x0000007fffff,
    0x000000ffffff,
    0x000001ffffff,
    0x000003ffffff,
    0x000007ffffff,
    0x00000fffffff,
    0x00001fffffff,
    0x00003fffffff,
    0x00007fffffff,
    0x0000ffffffff,
    0x0001ffffffff,
    0x0003ffffffff,
    0x0007ffffffff,
    0x000fffffffff,
    0x001fffffffff,
    0x003fffffffff,
    0x007fffffffff,
    0x00ffffffffff,
    0x01ffffffffff,
    0x03ffffffffff,
    0x07ffffffffff,
    0x0fffffffffff,
    0x1fffffffffff,
    0x3fffffffffff,
    0x7fffffffffff
    };

#if DEBUG
static FILE float180Log = NULL;
#endif

/*
**  Condition action definitions for user conditions, indexed by UserCondition
*/
static u16 ucrMasks [] =
    {
    0x8000, /* UCR48 Privileged instruction fault     */
    0x4000, /* UCR49 Unimplemented instruction        */
    0x2000, /* UCR50 Free flag                        */
    0x1000, /* UCR51 Process interval timer           */
    0x0800, /* UCR52 Inter-ring pop                   */
    0x0400, /* UCR53 Critical frame flag              */
    0x0200, /* UCR54 Reserved                         */
    0x0100, /* UCR55 Divide fault                     */
    0x0080, /* UCR56 Debug                            */
    0x0040, /* UCR57 Arithmetic overflow              */
    0x0020, /* UCR58 Exponent overflow                */
    0x0010, /* UCR59 Exponent underflow               */
    0x0008, /* UCR60 FP loss of significance          */
    0x0004, /* UCR61 FP indefinite                    */
    0x0002, /* UCR62 Arithmetic loss of significance  */
    0x0001  /* UCR63 Invalid BDP data                 */
    };

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Add two double precision floating point quantities and
**                  detect exceptions
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  augend      the augend
**                  addend      the addend
**                  sum         (out) the sum
**
**  Returns:        TRUE if no exceptions detected or instruction should
**                  complete. FALSE if exception detected and instruction
**                  should be inhibited.
**
**                  UCR set if exception detected.
**
**------------------------------------------------------------------------*/
bool float180AddDouble(Cpu180Context *ctx, Cpu180Double *augend, Cpu180Double *addend, Cpu180Double *sum)
    {
    FloatClass   classAddend;
    FloatClass   classAugend;
    Cpu180Double coeffAddend;
    Cpu180Double coeffAugend;
    Cpu180Double coeffResult;
    u16          expAddend;
    u16          expAugend;
    u16          expResult;
    u64          nextP;
    u16          shift;
    u8           signAddend;
    u8           signAugend;
    u8           signResult;

    classAddend = float180DoubleClassOf(addend);
    classAugend = float180DoubleClassOf(augend);
    switch ((classAugend * 5) + classAddend)
        {
    default:
    case NxN:
    case NxZ3:
    case Z3xN:
        expAddend             = ExponentOf(addend->leftPart);
        coeffAddend.leftPart  = CoefficientOf(addend->leftPart);
        coeffAddend.rightPart = CoefficientOf(addend->rightPart);
        signAddend            = SignOf(addend->leftPart);
        expAugend             = ExponentOf(augend->leftPart);
        coeffAugend.leftPart  = CoefficientOf(augend->leftPart);
        coeffAugend.rightPart = CoefficientOf(augend->rightPart);
        signAugend            = SignOf(augend->leftPart);
        if (expAddend > expAugend)
            {
            expResult = expAddend;
            shift     = expAddend - expAugend;
            if (shift > 96)
                {
                shift = 96;
                }
            if (shift < 48)
                {
                coeffAugend.rightPart  = (coeffAugend.rightPart >> shift) | ((coeffAugend.leftPart & coeffMasks[shift]) << (48 - shift));
                coeffAugend.leftPart >>= shift;
                }
            else
                {
                coeffAugend.rightPart = coeffAugend.leftPart >> (shift - 48);
                coeffAugend.leftPart  = 0;
                }
            }
        else if (expAugend > expAddend)
            {
            expResult = expAugend;
            shift     = expAugend - expAddend;
            if (shift > 96)
                {
                shift = 96;
                }
            if (shift < 48)
                {
                coeffAddend.rightPart  = (coeffAddend.rightPart >> shift) | ((coeffAddend.leftPart & coeffMasks[shift]) << (48 - shift));
                coeffAddend.leftPart >>= shift;
                }
            else
                {
                coeffAddend.rightPart = coeffAddend.leftPart >> (shift - 48);
                coeffAddend.leftPart  = 0;
                }
            }
        else
            {
            expResult = expAddend;
            }
	if (signAugend == signAddend)
	    {
            signResult            = signAugend;
            coeffResult.leftPart  = coeffAugend.leftPart + coeffAddend.leftPart;
            coeffResult.rightPart = coeffAugend.rightPart + coeffAddend.rightPart;
            if (coeffResult.rightPart > 0xffffffffffff)
                {
                coeffResult.rightPart &= 0xffffffffffff;
                coeffResult.leftPart  += 1;
                }
            }
        else if (signAddend != 0)
            {
            if (coeffAddend.leftPart > coeffAugend.leftPart
                || (coeffAddend.leftPart == coeffAugend.leftPart && coeffAddend.rightPart >= coeffAugend.rightPart))
                {
                signResult = signAddend;
                }
            else
                {
                signResult = signAugend;
                }
            coeffResult.leftPart  = coeffAugend.leftPart - coeffAddend.leftPart;
            coeffResult.rightPart = coeffAugend.rightPart - coeffAddend.rightPart;
            if (coeffResult.rightPart > 0xffffffffffff)
                {
                coeffResult.leftPart  -= 1;
                coeffResult.rightPart &= 0xffffffffffff;
                }
            if ((coeffResult.leftPart >> 63) == 1)
                {
                coeffResult.leftPart  = ~coeffResult.leftPart & 0xffffffffffff;
                coeffResult.rightPart = (~coeffResult.rightPart & 0xffffffffffff) + 1;
                if (coeffResult.rightPart > 0xffffffffffff)
                    {
                    coeffResult.rightPart &= 0xffffffffffff;
                    coeffResult.leftPart   = (coeffResult.leftPart + 1) & 0xffffffffffff;
                    }
                }
            }
        else
            {
            if (coeffAugend.leftPart > coeffAddend.leftPart
                || (coeffAugend.leftPart == coeffAddend.leftPart && coeffAugend.rightPart >= coeffAddend.rightPart))
                {
                signResult = signAugend;
                }
            else
                {
                signResult = signAddend;
                }
            coeffResult.leftPart  = coeffAddend.leftPart - coeffAugend.leftPart;
            coeffResult.rightPart = coeffAddend.rightPart - coeffAugend.rightPart;
            if (coeffResult.rightPart > 0xffffffffffff)
                {
                coeffResult.leftPart  -= 1;
                coeffResult.rightPart &= 0xffffffffffff;
                }
            if ((coeffResult.leftPart >> 63) == 1)
                {
                coeffResult.leftPart  = ~coeffResult.leftPart & 0xffffffffffff;
                coeffResult.rightPart = (~coeffResult.rightPart & 0xffffffffffff) + 1;
                if (coeffResult.rightPart > 0xffffffffffff)
                    {
                    coeffResult.rightPart &= 0xffffffffffff;
                    coeffResult.leftPart   = (coeffResult.leftPart + 1) & 0xffffffffffff;
                    }
                }
            }
        break;
    //
    //  See MIGDS 2-92 and 2-93
    //
    case Z1Z2xN:
        expResult             = ExponentOf(addend->leftPart);
        coeffResult.leftPart  = CoefficientOf(addend->leftPart);
        coeffResult.rightPart = CoefficientOf(addend->rightPart);
        signResult            = SignOf(addend->leftPart);
        break;
    case NxZ1Z2:
        expResult             = ExponentOf(augend->leftPart);
        coeffResult.leftPart  = CoefficientOf(augend->leftPart);
        coeffResult.rightPart = CoefficientOf(augend->rightPart);
        signResult            = SignOf(augend->leftPart);
        break;
    case Z1Z2xZ1Z2:
        sum->leftPart  = 0;
        sum->rightPart = 0;
        return TRUE;
    case Z3xZ3:
        nextP = ctx->nextP;
        cpu180SetUserCondition(ctx, UCR60); // FP loss of significance
        if (IsUmrBitSet(ctx, UCR59))
            {
            expAugend      = ExponentOf(augend->leftPart);
            expAddend      = ExponentOf(addend->leftPart);
            sum->leftPart  = expAugend > expAddend ? (u64)expAugend << 48 : (u64)expAddend << 48;
            sum->rightPart = sum->leftPart;
            ctx->nextP = nextP;
            }
        else
            {
            sum->leftPart  = 0;
            sum->rightPart = 0;
            }
        return TRUE;
    case Z1Z2xZ3:
    case Z3xZ1Z2:
        nextP = ctx->nextP;
        cpu180SetUserCondition(ctx, UCR60); // FP loss of significance
        if (IsUmrBitSet(ctx, UCR59))
            {
            expAugend      = ExponentOf(augend->leftPart);
            expAddend      = ExponentOf(addend->leftPart);
            sum->leftPart  = IsStandard(expAugend) ? (u64)expAugend << 48 : (u64)expAddend << 48;
            sum->rightPart = sum->leftPart;
            ctx->nextP = nextP;
            }
        else
            {
            sum->leftPart  = 0;
            sum->rightPart = 0;
            }
        return TRUE;
    case Z1Z2xINDEF:
    case Z3xINDEF:
    case NxINDEF:
    case INFxINDEF:
    case INDEFxZ1Z2:
    case INDEFxZ3:
    case INDEFxN:
    case INDEFxINDEF:
    case INDEFxINF:
        cpu180SetUserCondition(ctx, UCR61);
        sum->leftPart  = INDEFINITE;
        sum->rightPart = INDEFINITE;
        if (IsUcTrapEnabled(ctx, UCR61)) // inhibit instruction execution
            {
            return FALSE;
            }
        return TRUE;
    case INFxINF:
        signAugend = SignOf(augend->leftPart);
        signAddend = SignOf(addend->leftPart);
        if (signAugend != signAddend)
            {
            cpu180SetUserCondition(ctx, UCR61);
            sum->leftPart  = INDEFINITE;
            sum->rightPart = INDEFINITE;
            if (IsUcTrapEnabled(ctx, UCR61)) // inhibit instruction execution
                {
                return FALSE;
                }
            }
        else
            {
            cpu180SetUserCondition(ctx, UCR58);
            sum->leftPart  = ((u64)signAugend << 63) | INFINITE;
            sum->rightPart = sum->leftPart;
            }
        return TRUE;
    case Z1Z2xINF:
    case Z3xINF:
    case NxINF:
        cpu180SetUserCondition(ctx, UCR58);
        sum->leftPart  = (SignOf(addend->leftPart) << 63) | INFINITE;
        sum->rightPart = sum->leftPart;
        return TRUE;
    case INFxZ1Z2:
    case INFxZ3:
    case INFxN:
        cpu180SetUserCondition(ctx, UCR58);
        sum->leftPart  = (SignOf(augend->leftPart) << 63) | INFINITE;
        sum->rightPart = sum->leftPart;
        return TRUE;
        }

    if (coeffResult.leftPart != 0 || coeffResult.rightPart != 0)
        {
        float180NormalizeDouble(&expResult, &coeffResult);
        sum->leftPart  = ((u64)signResult << 63) | ((u64)expResult << 48) | coeffResult.leftPart;
        sum->rightPart = ((u64)signResult << 63) | ((u64)expResult << 48) | coeffResult.rightPart;
        if (IsInfinite(expResult)) // exponent overflow
            {
            nextP = ctx->nextP;
            cpu180SetUserCondition(ctx, UCR58);
            if (IsUmrBitSet(ctx, UCR58))
                {
                ctx->nextP = nextP;
                }
            else
                {
                sum->leftPart  = ((u64)signResult << 63) | INFINITE;
                sum->rightPart = ((u64)signResult << 63) | INFINITE;
                }
            }
        else if (IsZ2(expResult))  // exponent underflow
            {
            nextP = ctx->nextP;
            cpu180SetUserCondition(ctx, UCR59);
            if (IsUmrBitSet(ctx, UCR59))
                {
                ctx->nextP = nextP;
                }
            else
                {
                sum->leftPart  = 0;
                sum->rightPart = 0;
                }
            }
        }
    else
        {
        nextP = ctx->nextP;
        cpu180SetUserCondition(ctx, UCR60); // FP loss of significance
        if (IsUmrBitSet(ctx, UCR60))
            {
            sum->leftPart  = (u64)expResult << 48; // Z3
            sum->rightPart = sum->leftPart;
            ctx->nextP = nextP;
            }
        else
            {
            sum->leftPart  = 0;
            sum->rightPart = 0;
            }
        }
    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Add two single precision floating point quantities and
**                  detect exceptions
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  augend      the augend
**                  addend      the addend
**                  sum         (out) the sum
**
**  Returns:        TRUE if no exceptions detected or instruction should
**                  complete. FALSE if exception detected and instruction
**                  should be inhibited.
**
**                  UCR set if exception detected.
**
**------------------------------------------------------------------------*/
bool float180AddFloat(Cpu180Context *ctx, u64 augend, u64 addend, u64 *sum)
    {
    FloatClass classAddend;
    FloatClass classAugend;
    u64        coeffAddend;
    u64        coeffAugend;
    u64        coeffResult;
    u16        expAddend;
    u16        expAugend;
    u16        expResult;
    u64        nextP;
    u16        shift;
    u8         signAddend;
    u8         signAugend;
    u8         signResult;

    classAddend = float180FloatClassOf(addend);
    classAugend = float180FloatClassOf(augend);
    switch ((classAugend * 5) + classAddend)
        {
    default:
    case NxN:
    case NxZ3:
    case Z3xN:
        expAddend   = ExponentOf(addend);
        coeffAddend = CoefficientOf(addend);
        signAddend  = SignOf(addend);
        expAugend   = ExponentOf(augend);
        coeffAugend = CoefficientOf(augend);
        signAugend  = SignOf(augend);
        if (expAddend > expAugend)
            {
            expResult = expAddend;
            shift     = expAddend - expAugend;
            if (shift > 48)
                {
                shift = 48;
                }
            coeffAugend >>= shift;
            }
        else if (expAugend > expAddend)
            {
            expResult = expAugend;
            shift     = expAugend - expAddend;
            if (shift > 48)
                {
                shift = 48;
                }
            coeffAddend >>= shift;
            }
        else
            {
            expResult = expAddend;
            }
	if (signAugend == signAddend)
	    {
            signResult  = signAugend;
            coeffResult = coeffAugend + coeffAddend;
            }
        else if (signAddend != 0)
            {
            signResult  = (coeffAddend > coeffAugend) ? signAddend : signAugend;
            coeffResult = coeffAugend - coeffAddend;
            if ((coeffResult >> 63) == 1)
                {
                coeffResult = ~coeffResult + 1;
                }
            }
        else
            {
            signResult  = (coeffAddend > coeffAugend) ? signAddend : signAugend;
            coeffResult = coeffAddend - coeffAugend;
            if ((coeffResult >> 63) == 1)
                {
                coeffResult = ~coeffResult + 1;
                }
            }
        break;
    //
    //  See MIGDS 2-92 and 2-93
    //
    case Z1Z2xN:
        expResult   = ExponentOf(addend);
        coeffResult = CoefficientOf(addend);
        signResult  = SignOf(addend);
        break;
    case NxZ1Z2:
        expResult   = ExponentOf(augend);
        coeffResult = CoefficientOf(augend);
        signResult  = SignOf(augend);
        break;
    case Z1Z2xZ1Z2:
        *sum = 0;
        return TRUE;
    case Z3xZ3:
        nextP = ctx->nextP;
        cpu180SetUserCondition(ctx, UCR60); // FP loss of significance
        if (IsUmrBitSet(ctx, UCR59))
            {
            expAugend = ExponentOf(augend);
            expAddend = ExponentOf(addend);
            *sum = expAugend > expAddend ? (u64)expAugend << 48 : (u64)expAddend << 48;
            ctx->nextP = nextP;
            }
        else
            {
            *sum = 0;
            }
        return TRUE;
    case Z1Z2xZ3:
    case Z3xZ1Z2:
        nextP = ctx->nextP;
        cpu180SetUserCondition(ctx, UCR60); // FP loss of significance
        if (IsUmrBitSet(ctx, UCR59))
            {
            expAugend = ExponentOf(augend);
            expAddend = ExponentOf(addend);
            *sum = IsStandard(expAugend) ? (u64)expAugend << 48 : (u64)expAddend << 48;
            ctx->nextP = nextP;
            }
        else
            {
            *sum = 0;
            }
        return TRUE;
    case Z1Z2xINDEF:
    case Z3xINDEF:
    case NxINDEF:
    case INFxINDEF:
    case INDEFxZ1Z2:
    case INDEFxZ3:
    case INDEFxN:
    case INDEFxINDEF:
    case INDEFxINF:
        cpu180SetUserCondition(ctx, UCR61);
        *sum = INDEFINITE;
        if (IsUcTrapEnabled(ctx, UCR61)) // inhibit instruction execution
            {
            return FALSE;
            }
        return TRUE;
    case INFxINF:
        signAugend = SignOf(augend);
        signAddend = SignOf(addend);
        if (signAugend != signAddend)
            {
            cpu180SetUserCondition(ctx, UCR61);
            *sum = INDEFINITE;
            if (IsUcTrapEnabled(ctx, UCR61)) // inhibit instruction execution
                {
                return FALSE;
                }
            }
        else
            {
            cpu180SetUserCondition(ctx, UCR58);
            *sum = ((u64)signAugend << 63) | INFINITE;
            }
        return TRUE;
    case Z1Z2xINF:
    case Z3xINF:
    case NxINF:
        cpu180SetUserCondition(ctx, UCR58);
        *sum = (SignOf(addend) << 63) | INFINITE;
        return TRUE;
    case INFxZ1Z2:
    case INFxZ3:
    case INFxN:
        cpu180SetUserCondition(ctx, UCR58);
        *sum = (SignOf(augend) << 63) | INFINITE;
        return TRUE;
        }

    if (coeffResult != 0)
        {
        float180NormalizeFloat(&expResult, &coeffResult);
        *sum = ((u64)signResult << 63) | ((u64)expResult << 48) | coeffResult;
        if (IsInfinite(expResult)) // exponent overflow
            {
            nextP = ctx->nextP;
            cpu180SetUserCondition(ctx, UCR58);
            if (IsUmrBitSet(ctx, UCR58))
                {
                ctx->nextP = nextP;
                }
            else
                {
                *sum = ((u64)signResult << 63) | INFINITE;
                }
            }
        else if (IsZ2(expResult))  // exponent underflow
            {
            nextP = ctx->nextP;
            cpu180SetUserCondition(ctx, UCR59);
            if (IsUmrBitSet(ctx, UCR59))
                {
                ctx->nextP = nextP;
                }
            else
                {
                *sum = 0;
                }
            }
        }
    else
        {
        nextP = ctx->nextP;
        cpu180SetUserCondition(ctx, UCR60); // FP loss of significance
        if (IsUmrBitSet(ctx, UCR60))
            {
            *sum = (u64)expResult << 48; // Z3
            ctx->nextP = nextP;
            }
        else
            {
            *sum = 0;
            }
        }
    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Compare two single precision floating point quantities
**                  and detect exceptions
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  minend      the minuend
**                  subend      the subtrahend
**                  valence     (out) -1 if minend <  subend
**                                     0 if minend == subend
**                                     1 if minend >  subend
**
**  Returns:        TRUE if no exceptions detected or instruction should
**                  complete. FALSE if exception detected and instruction
**                  should be inhibited.
**
**                  UCR set if exception detected.
**
**------------------------------------------------------------------------*/
bool float180CompareFloat(Cpu180Context *ctx, u64 minend, u64 subend, int *valence)
    {
    ConditionAction action;
    FloatClass      classSubend;
    FloatClass      classMinend;
    u64             diff;
    bool            isOk;
    u64             nextP;
    u8              signSubend;
    u8              signMinend;
    u16             ucr;
    u16             ucrDelta;
    u16             umr;

    classSubend = float180FloatClassOf(subend);
    classMinend = float180FloatClassOf(minend);
    switch ((classMinend * 5) + classSubend)
        {
    default:
    case NxN:
    case Z3xN:
    case NxZ3:
    case Z3xZ3:
        signMinend = SignOf(minend);
        signSubend = SignOf(subend);
        if (signMinend < signSubend)
            {
            *valence = 1;
            return TRUE;
            }
        else if (signMinend > signSubend)
            {
            *valence = -1;
            return TRUE;
            }
        /*
        **  MIGDS 2-89 says "For standard numbers having like signs
        **  a floating point subtract shall be performed in the manner
        **  described in subparagraph 2.4.3.1 of this specification,
        **  with the exception that the operation is performed as if
        **  the (FP Overflow, Underflow and Loss of Significance)
        **  User Mask bits were set (Z2 not forced to zero, etc.) and
        **  that the result shall not be transferred to Register Xk
        **  but shall be interpreted in its post-normalized form to
        **  determine the result of the comparison."
        **
        **  The UMR/UCR mask for FP Overflow, Underflow, and Loss of
        **  significance is 0x0038.
        */
        ucr          = ctx->regUcr;
        umr          = ctx->regUmr;
        action       = ctx->pendingAction;
        nextP        = ctx->nextP;
        ctx->regUmr |= 0x0038;  // set all of the bits temporarily
        isOk         = float180SubFloat(ctx, minend, subend, &diff);
        ctx->regUmr  = umr;  // restore original mask
        ucrDelta     = (ctx->regUcr ^ ucr) & 0x0038;
        if (ucrDelta != 0)
            {
            ctx->regUcr       &= ~ucrDelta;
            ctx->pendingAction = action;
            ctx->nextP         = nextP;
            }
        if (isOk)
            {
            if (diff == 0 || IsZ3(ExponentOf(diff), CoefficientOf(diff)))
                {
                *valence = 0;
                }
            else if ((diff & 0x8000000000000000) == 0)
                {
                *valence = 1;
                }
            else
                {
                *valence = -1;
                }
            return TRUE;
            }
        return FALSE;
    case NxZ1Z2:
    case Z3xZ1Z2:
        *valence = (SignOf(minend) == 0) ? 1 : -1;
        return TRUE;
    case Z1Z2xN:
    case Z1Z2xZ3:
    case Z1Z2xINF:
        *valence = (SignOf(subend) == 0) ? -1 : 1;
        return TRUE;
    case Z1Z2xZ1Z2:
        *valence = 0;
        return TRUE;
    case NxINF:
    case Z3xINF:
        *valence = (SignOf(subend) == 0) ? -1 : 1;
        return TRUE;
    case Z1Z2xINDEF:
    case Z3xINDEF:
    case NxINDEF:
    case INFxINDEF:
    case INDEFxZ1Z2:
    case INDEFxZ3:
    case INDEFxN:
    case INDEFxINDEF:
    case INDEFxINF:
        cpu180SetUserCondition(ctx, UCR61);
        *valence = 0;
        return FALSE;
    case INFxZ3:
    case INFxN:
    case INFxZ1Z2:
        *valence = (SignOf(minend) == 0) ? 1 : -1;
        return TRUE;
    case INFxINF:
        signMinend = SignOf(minend);
        signSubend = SignOf(subend);
        if (signMinend < signSubend)
            {
            *valence = 1;
            }
        else if (signMinend > signSubend)
            {
            *valence = -1;
            }
        else
            {
            cpu180SetUserCondition(ctx, UCR61);
            *valence = 0;
            return FALSE;
            }
        return TRUE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert a floating point value to an integer
**
**  Parameters:     Name         Description.
**                  floatValue   the floating point value to convert
**                  intResult    (out) the integer result
**
**  Returns:        TRUE if successful. FALSE if exception detected, and
**                  UCR set to reflect exception and trap enabled
**
**------------------------------------------------------------------------*/
bool float180ConvertFloatToInt(Cpu180Context *ctx, u64 floatValue, u64 *intResult)
    {
    u64 coefficient;
    u16 exponent;
    u16 shift;

    *intResult  = 0;
    exponent    = ExponentOf(floatValue);
    coefficient = CoefficientOf(floatValue);

    if (IsIndefinite(exponent))
        {
        if (IsUcTrapEnabled(ctx, UCR61))
            {
            cpu180SetUserCondition(ctx, UCR61); // FP indefinite
            return FALSE;
            }
        ctx->regUcr |= UcrBitMask(UCR61);
        }
    else if (IsInfinite(exponent))
        {
        if (IsUcTrapEnabled(ctx, UCR62))
            {
            cpu180SetUserCondition(ctx, UCR62); // Arithmetic loss of significance
            return FALSE;
            }
        ctx->regUcr |= UcrBitMask(UCR62);
        }
    else if (coefficient != 0)
        {
        float180NormalizeFloat(&exponent, &coefficient);
        shift = exponent - BIAS;
        if (shift > 0 && shift < 0x8000U)
            {
            if (shift < 48)
                {
                *intResult = coefficient >> (48 - shift);
                }
            else if (shift > 48)
                {
                shift -= 48;
                if (shift < 16)
                    {
                    *intResult = coefficient << shift;
                    }
                else
                    {
                    if (IsUcTrapEnabled(ctx, UCR62))
                        {
                        cpu180SetUserCondition(ctx, UCR62); // Arithmetic loss of significance
                        return FALSE;
                        }
                    ctx->regUcr |= UcrBitMask(UCR62);
                    if (shift < 64)
                        {
                        *intResult = coefficient << shift;
                        }
                    }
                }
            if (SignOf(floatValue) != 0)
                {
                *intResult = ~*intResult + 1;
                }
            }
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert an integer value to floating point
**
**  Parameters:     Name         Description.
**                  intValue     the floating point value to convert
**
**  Returns:        Float point value
**
**------------------------------------------------------------------------*/
u64 float180ConvertIntToFloat(u64 intValue)
    {
    u16 exponent;
    u64 floatResult;
    u8  sign;

    if (intValue == 0)
        {
        floatResult = 0;
        }
    else
        {
        sign = intValue >> 63;
        if (sign != 0)
            {
            intValue = ~intValue + 1;
            }
        exponent = BIAS + 48;
        float180NormalizeFloat(&exponent, &intValue);
        floatResult = ((u64)sign << 63) | ((u64)exponent << 48) | intValue;
        }

    return floatResult;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Divide two double precision floating point quantities
**                  and detect exceptions
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  mltand      the dividend
**                  mltier      the divisor
**                  quotient    (out) the quotient
**
**  Returns:        TRUE if no exceptions detected or instruction should
**                  complete. FALSE if exception detected and instruction
**                  should be inhibited.
**
**                  UCR set if exception detected.
**
**------------------------------------------------------------------------*/
bool float180DivDouble(Cpu180Context *ctx, Cpu180Double *dvdend, Cpu180Double *dvisor, Cpu180Double *quotient)
    {
    FloatClass   classDvisor;
    FloatClass   classDvdend;
    Cpu180Double coeffDvisor;
    Cpu180Double coeffDvdend;
    Cpu180Double coeffResult;
    Cpu180Double coeffTmp;
    u16          expDvisor;
    u16          expDvdend;
    u16          expResult;
    u8           signDvisor;
    u8           signDvdend;
    u8           signResult;

    classDvisor = float180DoubleClassOf(dvisor);
    classDvdend = float180DoubleClassOf(dvdend);
    switch ((classDvdend * 5) + classDvisor)
        {
    default:
    case NxN:
        expDvisor             = ExponentOf(dvisor->leftPart) - BIAS;
        coeffDvisor.leftPart  = CoefficientOf(dvisor->leftPart);
        coeffDvisor.rightPart = CoefficientOf(dvisor->rightPart);
        signDvisor            = SignOf(dvisor->leftPart);
        expDvdend             = ExponentOf(dvdend->leftPart) - BIAS;
        coeffDvdend.leftPart  = CoefficientOf(dvdend->leftPart);
        coeffDvdend.rightPart = CoefficientOf(dvdend->rightPart);
        signDvdend            = SignOf(dvdend->leftPart);
        expResult             = (expDvdend - expDvisor) + BIAS;
        signResult            = signDvisor ^ signDvdend;
        if ((coeffDvisor.leftPart & 0x800000000000) == 0) // unnormalized divisor
            {
            coeffTmp.leftPart  = (coeffDvisor.leftPart << 1) | ((coeffDvisor.rightPart >> 47) & 1);
            coeffTmp.rightPart = (coeffDvisor.rightPart << 1) & Mask48;
            if (coeffTmp.leftPart < coeffDvdend.leftPart
                || (coeffTmp.leftPart == coeffDvdend.leftPart && coeffTmp.rightPart <= coeffDvdend.rightPart))
                {
                *quotient = *dvdend;
                cpu180SetUserCondition(ctx, UCR55); // divide fault
                if (IsUmrBitSet(ctx, UCR55))
                    {
                    return FALSE;
                    }
                return TRUE;
                }
            }
        float180LongDiv(&coeffDvdend, &coeffDvisor, &coeffResult);
        if (coeffResult.leftPart > 0xffffffffffff)
            {
            coeffResult.rightPart = (coeffResult.rightPart >> 1) | ((coeffResult.leftPart & 1) << 47);
            coeffResult.leftPart  = ((coeffResult.leftPart >> 1) | 0x800000000000) & Mask48;
            expResult            += 1;
            }
        quotient->leftPart  = ((u64)signResult << 63) | ((u64)expResult << 48) | coeffResult.leftPart;
        quotient->rightPart = ((u64)signResult << 63) | ((u64)expResult << 48) | coeffResult.rightPart;
        if (IsInfinite(expResult)) // exponent overflow
            {
            cpu180SetUserCondition(ctx, UCR58);
            if (IsUmrBitSet(ctx, UCR58) == FALSE)
                {
                quotient->leftPart  = ((u64)signResult << 63) | INFINITE;
                quotient->rightPart = ((u64)signResult << 63) | INFINITE;
                }
            }
        else if (IsZ2(expResult))  // exponent underflow
            {
            cpu180SetUserCondition(ctx, UCR59);
            if (IsUmrBitSet(ctx, UCR59) == FALSE)
                {
                quotient->leftPart  = 0;
                quotient->rightPart = 0;
                }
            }
        return TRUE;
    case Z3xN:
        expDvisor             = ExponentOf(dvisor->leftPart) - BIAS;
        signDvisor            = SignOf(dvisor->leftPart);
        expDvdend             = ExponentOf(dvdend->leftPart) - BIAS;
        signDvdend            = SignOf(dvdend->leftPart);
        expResult             = (expDvdend - expDvisor) + BIAS;
        signResult            = signDvisor ^ signDvdend;
        quotient->leftPart  = ((u64)signResult << 63) | ((u64)expResult << 48);
        quotient->rightPart = quotient->leftPart;
        if (IsInfinite(expResult)) // exponent overflow
            {
            cpu180SetUserCondition(ctx, UCR58);
            if (IsUmrBitSet(ctx, UCR58) == FALSE)
                {
                quotient->leftPart  = ((u64)signResult << 63) | INFINITE;
                quotient->rightPart = ((u64)signResult << 63) | INFINITE;
                }
            }
        else if (IsZ2(expResult))  // exponent underflow
            {
            cpu180SetUserCondition(ctx, UCR59);
            if (IsUmrBitSet(ctx, UCR59) == FALSE)
                {
                quotient->leftPart  = 0;
                quotient->rightPart = 0;
                }
            }
        return TRUE;
    //
    //  See MIGDS 2-98 and 2-99
    //
    case Z1Z2xZ3:
    case Z1Z2xN:
    case Z1Z2xINF:
    case Z3xINF:
    case NxINF:
        quotient->leftPart  = 0;
        quotient->rightPart = 0;
        return TRUE;
    case Z1Z2xZ1Z2:
    case Z3xZ1Z2:
    case Z3xZ3:
    case NxZ1Z2:
    case NxZ3:
    case INFxZ1Z2:
    case INDEFxZ1Z2:
        *quotient = *dvdend;
        cpu180SetUserCondition(ctx, UCR55); // divide fault
        if (IsUmrBitSet(ctx, UCR55))
            {
            return FALSE;
            }
        return TRUE;
    case INFxZ3:
    case INFxN:
        cpu180SetUserCondition(ctx, UCR58);
        quotient->leftPart  = ((SignOf(dvdend->leftPart) ^ SignOf(dvisor->leftPart)) << 63) | INFINITE;
        quotient->rightPart = ((SignOf(dvdend->leftPart) ^ SignOf(dvisor->leftPart)) << 63) | INFINITE;
        return TRUE;
    case Z1Z2xINDEF:
    case Z3xINDEF:
    case NxINDEF:
    case INFxINF:
    case INFxINDEF:
    case INDEFxZ3:
    case INDEFxN:
    case INDEFxINDEF:
    case INDEFxINF:
        cpu180SetUserCondition(ctx, UCR61);
        quotient->leftPart  = INDEFINITE;
        quotient->rightPart = INDEFINITE;
        if (IsUcTrapEnabled(ctx, UCR61)) // inhibit instruction execution
            {
            return FALSE;
            }
        return TRUE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Divide two single precision floating point quantities
**                  and detect exceptions
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  mltand      the dividend
**                  mltier      the divisor
**                  quotient    (out) the quotient
**
**  Returns:        TRUE if no exceptions detected or instruction should
**                  complete. FALSE if exception detected and instruction
**                  should be inhibited.
**
**                  UCR set if exception detected.
**
**------------------------------------------------------------------------*/
bool float180DivFloat(Cpu180Context *ctx, u64 dvdend, u64 dvisor, u64 *quotient)
    {
    FloatClass classDvisor;
    FloatClass classDvdend;
    u64        coeffDvisor;
    u64        coeffDvdend;
    u64        coeffResult;
    u16        expDvisor;
    u16        expDvdend;
    u16        expResult;
    u8         signDvisor;
    u8         signDvdend;
    u8         signResult;
#if defined(_WIN32)
    u64        remainder;
#endif

    classDvisor = float180FloatClassOf(dvisor);
    classDvdend = float180FloatClassOf(dvdend);
    switch ((classDvdend * 5) + classDvisor)
        {
    default:
    case Z3xN:
    case NxN:
        expDvisor   = ExponentOf(dvisor) - BIAS;
        coeffDvisor = CoefficientOf(dvisor);
        signDvisor  = SignOf(dvisor);
        expDvdend   = ExponentOf(dvdend) - BIAS;
        coeffDvdend = CoefficientOf(dvdend);
        signDvdend  = SignOf(dvdend);
        expResult   = (expDvdend - expDvisor) + BIAS;
        signResult  = signDvisor ^ signDvdend;
        if ((coeffDvisor & 0x800000000000) == 0) // unnormalized divisor
            {
            if ((coeffDvisor << 1) <= coeffDvdend) // divisor <= half of dividend
                {
                *quotient = dvdend;
                cpu180SetUserCondition(ctx, UCR55); // divide fault
                if (IsUmrBitSet(ctx, UCR55))
                    {
                    return FALSE;
                    }
                return TRUE;
                }
            }
#if defined(_WIN32)
        coeffResult = _udiv128(coeffDvdend >> 16, (coeffDvdend & Mask16) << 48, coeffDvisor, &remainder);
#else
        coeffResult = (u64)(((u128)coeffDvdend << 48) / (u128)coeffDvisor);
#endif
        if (coeffResult > 0xffffffffffff)
            {
            coeffResult = (coeffResult >> 1) | 0x800000000000;
            expResult  += 1;
            }
        *quotient = ((u64)signResult << 63) | ((u64)expResult << 48) | coeffResult;
        if (IsInfinite(expResult)) // exponent overflow
            {
            cpu180SetUserCondition(ctx, UCR58);
            if (IsUmrBitSet(ctx, UCR58) == FALSE)
                {
                *quotient = ((u64)signResult << 63) | INFINITE;
                }
            }
        else if (IsZ2(expResult))  // exponent underflow
            {
            cpu180SetUserCondition(ctx, UCR59);
            if (IsUmrBitSet(ctx, UCR59) == FALSE)
                {
                *quotient = 0;
                }
            }
        return TRUE;
    //
    //  See MIGDS 2-98 and 2-99
    //
    case Z1Z2xZ3:
    case Z1Z2xN:
    case Z1Z2xINF:
    case Z3xINF:
    case NxINF:
        *quotient = 0;
        return TRUE;
    case Z1Z2xZ1Z2:
    case Z3xZ1Z2:
    case Z3xZ3:
    case NxZ1Z2:
    case NxZ3:
    case INFxZ1Z2:
    case INDEFxZ1Z2:
        *quotient = dvdend;
        cpu180SetUserCondition(ctx, UCR55); // divide fault
        if (IsUmrBitSet(ctx, UCR55))
            {
            return FALSE;
            }
        return TRUE;
    case INFxZ3:
    case INFxN:
        cpu180SetUserCondition(ctx, UCR58);
        *quotient = ((SignOf(dvdend) ^ SignOf(dvisor)) << 63) | INFINITE;
        return TRUE;
    case Z1Z2xINDEF:
    case Z3xINDEF:
    case NxINDEF:
    case INFxINF:
    case INFxINDEF:
    case INDEFxZ3:
    case INDEFxN:
    case INDEFxINDEF:
    case INDEFxINF:
        cpu180SetUserCondition(ctx, UCR61);
        *quotient = INDEFINITE;
        if (IsUcTrapEnabled(ctx, UCR61)) // inhibit instruction execution
            {
            return FALSE;
            }
        return TRUE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Multiply two double precision floating point quantities
**                  and detect exceptions
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  mltand      the multiplicand
**                  mltier      the multiplier
**                  product     (out) the product
**
**  Returns:        TRUE if no exceptions detected or instruction should
**                  complete. FALSE if exception detected and instruction
**                  should be inhibited.
**
**                  UCR set if exception detected.
**
**------------------------------------------------------------------------*/
bool float180MulDouble(Cpu180Context *ctx, Cpu180Double *mltand, Cpu180Double *mltier, Cpu180Double *product)
    {
    FloatClass   classMltier;
    FloatClass   classMltand;
    Cpu180Double coeffMltier;
    Cpu180Double coeffMltand;
    u16          expMltier;
    u16          expMltand;
    u16          expResult;
    Cpu180Double hiCoeffResult;
    Cpu180Double loCoeffResult;
    u8           signMltier;
    u8           signMltand;
    u8           signResult;

    classMltier = float180DoubleClassOf(mltier);
    classMltand = float180DoubleClassOf(mltand);
    switch ((classMltand * 5) + classMltier)
        {
    default:
    case Z3xZ3:
    case Z3xN:
    case NxN:
    case NxZ3:
        expMltier             = ExponentOf(mltier->leftPart) - BIAS;
        coeffMltier.leftPart  = CoefficientOf(mltier->leftPart);
        coeffMltier.rightPart = CoefficientOf(mltier->rightPart);
        signMltier            = SignOf(mltier->leftPart);
        expMltand             = ExponentOf(mltand->leftPart) - BIAS;
        coeffMltand.leftPart  = CoefficientOf(mltand->leftPart);
        coeffMltand.rightPart = CoefficientOf(mltand->rightPart);
        signMltand            = SignOf(mltand->leftPart);
        expResult             = (expMltier + expMltand) + BIAS;
        signResult            = signMltier ^ signMltand;
        float180LongMul(&coeffMltand, &coeffMltier, &hiCoeffResult, &loCoeffResult);
        if (hiCoeffResult.leftPart < 0x800000000000)
            {
            hiCoeffResult.leftPart  = (hiCoeffResult.leftPart << 1) | (hiCoeffResult.rightPart >> 47);
            hiCoeffResult.rightPart = ((hiCoeffResult.rightPart << 1) | (loCoeffResult.leftPart >> 47)) & Mask48;
            expResult              -= 1;
            }
        product->leftPart  = ((u64)signResult << 63) | ((u64)expResult << 48) | hiCoeffResult.leftPart;
        product->rightPart = ((u64)signResult << 63) | ((u64)expResult << 48) | hiCoeffResult.rightPart;
        if (IsInfinite(expResult)) // exponent overflow
            {
            cpu180SetUserCondition(ctx, UCR58);
            if (IsUmrBitSet(ctx, UCR58) == FALSE)
                {
                product->leftPart  = ((u64)signResult << 63) | INFINITE;
                product->rightPart = product->leftPart;
                }
            }
        else if (IsZ2(expResult))  // exponent underflow
            {
            cpu180SetUserCondition(ctx, UCR59);
            if (IsUmrBitSet(ctx, UCR59) == FALSE)
                {
                product->leftPart  = 0;
                product->rightPart = 0;
                }
            }
        return TRUE;
    //
    //  See MIGDS 2-96 and 2-97
    //
    case Z1Z2xZ1Z2:
    case Z1Z2xZ3:
    case Z1Z2xN:
    case Z3xZ1Z2:
    case NxZ1Z2:
        product->leftPart  = 0;
        product->rightPart = 0;
        return TRUE;
    case Z3xINF:
    case NxINF:
    case INFxZ3:
    case INFxN:
    case INFxINF:
        cpu180SetUserCondition(ctx, UCR58);
        product->leftPart  = ((SignOf(mltand->leftPart) ^ SignOf(mltier->leftPart)) << 63) | INFINITE;
        product->rightPart = product->leftPart;
        return TRUE;
    case Z1Z2xINDEF:
    case Z1Z2xINF:
    case Z3xINDEF:
    case NxINDEF:
    case INFxZ1Z2:
    case INFxINDEF:
    case INDEFxZ1Z2:
    case INDEFxZ3:
    case INDEFxN:
    case INDEFxINDEF:
    case INDEFxINF:
        cpu180SetUserCondition(ctx, UCR61);
        product->leftPart  = INDEFINITE;
        product->rightPart = INDEFINITE;
        if (IsUcTrapEnabled(ctx, UCR61)) // inhibit instruction execution
            {
            return FALSE;
            }
        return TRUE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Multiply two single precision floating point quantities
**                  and detect exceptions
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  mltand      the multiplicand
**                  mltier      the multiplier
**                  product     (out) the product
**
**  Returns:        TRUE if no exceptions detected or instruction should
**                  complete. FALSE if exception detected and instruction
**                  should be inhibited.
**
**                  UCR set if exception detected.
**
**------------------------------------------------------------------------*/
bool float180MulFloat(Cpu180Context *ctx, u64 mltand, u64 mltier, u64 *product)
    {
    FloatClass classMltier;
    FloatClass classMltand;
    u64        coeffMltier;
    u64        coeffMltand;
    u64        coeffResult;
    u16        expMltier;
    u16        expMltand;
    u16        expResult;
    u64        hiCoeff128;
    u64        loCoeff128;
    u8         signMltier;
    u8         signMltand;
    u8         signResult;
#if !defined(_WIN32)
    u128       p128;
#endif

    classMltier = float180FloatClassOf(mltier);
    classMltand = float180FloatClassOf(mltand);
    switch ((classMltand * 5) + classMltier)
        {
    default:
    case Z3xZ3:
    case Z3xN:
    case NxN:
    case NxZ3:
        expMltier   = ExponentOf(mltier) - BIAS;
        coeffMltier = CoefficientOf(mltier);
        signMltier  = SignOf(mltier);
        expMltand   = ExponentOf(mltand) - BIAS;
        coeffMltand = CoefficientOf(mltand);
        signMltand  = SignOf(mltand);
        expResult   = (expMltier + expMltand) + BIAS;
        signResult  = signMltier ^ signMltand;
#if defined(_WIN32)
        loCoeff128  = _umul128(coeffMltand, coeffMltier, &hiCoeff128);
#else
        p128        = (u128)coeffMltand * (u128)coeffMltier;
        loCoeff128  = (u64)p128;
        hiCoeff128  = (u64)(p128 >> 64);
#endif
        if (hiCoeff128 >= 0x80000000U)
            {
            coeffResult = (hiCoeff128 << 16) | (loCoeff128 >> 48);
            }
        else
            {
            coeffResult = (hiCoeff128 << 17) | (loCoeff128 >> 47);
            expResult  -= 1;
            }
        *product = ((u64)signResult << 63) | ((u64)expResult << 48) | coeffResult;
        if (IsInfinite(expResult)) // exponent overflow
            {
            cpu180SetUserCondition(ctx, UCR58);
            if (IsUmrBitSet(ctx, UCR58) == FALSE)
                {
                *product = ((u64)signResult << 63) | INFINITE;
                }
            }
        else if (IsZ2(expResult))  // exponent underflow
            {
            cpu180SetUserCondition(ctx, UCR59);
            if (IsUmrBitSet(ctx, UCR59) == FALSE)
                {
                *product = 0;
                }
            }
        return TRUE;
    //
    //  See MIGDS 2-96 and 2-97
    //
    case Z1Z2xZ1Z2:
    case Z1Z2xZ3:
    case Z1Z2xN:
    case Z3xZ1Z2:
    case NxZ1Z2:
        *product = 0;
        return TRUE;
    case Z3xINF:
    case NxINF:
    case INFxZ3:
    case INFxN:
    case INFxINF:
        cpu180SetUserCondition(ctx, UCR58);
        *product = ((SignOf(mltand) ^ SignOf(mltier)) << 63) | INFINITE;
        return TRUE;
    case Z1Z2xINDEF:
    case Z1Z2xINF:
    case Z3xINDEF:
    case NxINDEF:
    case INFxZ1Z2:
    case INFxINDEF:
    case INDEFxZ1Z2:
    case INDEFxZ3:
    case INDEFxN:
    case INDEFxINDEF:
    case INDEFxINF:
        cpu180SetUserCondition(ctx, UCR61);
        *product = INDEFINITE;
        if (IsUcTrapEnabled(ctx, UCR61)) // inhibit instruction execution
            {
            return FALSE;
            }
        return TRUE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Subtract two double precision floating point quantities and
**                  detect exceptions
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  minend      the minuend
**                  subend      the subtrahend
**                  diff        (out) the difference
**
**  Returns:        TRUE if no exceptions detected or instruction should
**                  complete. FALSE if exception detected and instruction
**                  should be inhibited.
**
**                  UCR set if exception detected.
**
**------------------------------------------------------------------------*/
bool float180SubDouble(Cpu180Context *ctx, Cpu180Double *minend, Cpu180Double *subend, Cpu180Double *diff)
    {
    Cpu180Double addend;

    addend.leftPart  = subend->leftPart ^ ((u64)1 << 63);
    addend.rightPart = subend->rightPart ^ ((u64)1 << 63);
    return float180AddDouble(ctx, minend, &addend, diff);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Subtract two single precision floating point quantities and
**                  detect exceptions
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  minend      the minuend
**                  subend      the subtrahend
**                  diff        (out) the difference
**
**  Returns:        TRUE if no exceptions detected or instruction should
**                  complete. FALSE if exception detected and instruction
**                  should be inhibited.
**
**                  UCR set if exception detected.
**
**------------------------------------------------------------------------*/
bool float180SubFloat(Cpu180Context *ctx, u64 minend, u64 subend, u64 *diff)
    {
    return float180AddFloat(ctx, minend, subend ^ ((u64)1 << 63), diff);
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Determine the class of a double precision value
**
**  Parameters:     Name        Description.
**                  doubleValue the value for which to determine the class
**
**  Returns:        Floating point class.
**
**------------------------------------------------------------------------*/
static FloatClass float180DoubleClassOf(Cpu180Double *value)
    {
    switch (value->leftPart >> 60)
        {
    case 0x7:
    case 0xf:
        return FloatClass_Indefinite;
    case 0x6:
    case 0x5:
    case 0xd:
    case 0xe:
        return FloatClass_Infinite;
    case 0x2:
    case 0x1:
    case 0x0:
    case 0x8:
    case 0x9:
    case 0xa:
        return FloatClass_Z1Z2;
    default:
        return ((value->leftPart | value->rightPart) & Mask48) != 0 ? FloatClass_N : FloatClass_Z3;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Determine the class of a single precision value
**
**  Parameters:     Name        Description.
**                  floatValue  the value for which to determine the class
**
**  Returns:        Floating point class.
**
**------------------------------------------------------------------------*/
static FloatClass float180FloatClassOf(u64 value)
    {
    switch (value >> 60)
        {
    case 0x7:
    case 0xf:
        return FloatClass_Indefinite;
    case 0x6:
    case 0x5:
    case 0xd:
    case 0xe:
        return FloatClass_Infinite;
    case 0x2:
    case 0x1:
    case 0x0:
    case 0x8:
    case 0x9:
    case 0xa:
        return FloatClass_Z1Z2;
    default:
        return (value & Mask48) != 0 ? FloatClass_N : FloatClass_Z3;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform long division of double precision coefficients
**
**  Parameters:     Name         Description.
**                  dvdend       pointer to 96-bit dividend
**                  dvisor       pointer to 96-bit divisor
**                  quotient     pointer to quotient
**
**------------------------------------------------------------------------*/
static void float180LongDiv(Cpu180Double *dvdend, Cpu180Double *dvisor, Cpu180Double *quotient)
    {
    u64 borrow;
    u64 diff;
    u64 dvdend192[4];
    u64 dvisor192[4];
    int i;
    u64 t[4];

    dvdend192[0]        = dvdend->leftPart & Mask48;
    dvdend192[1]        = dvdend->rightPart & Mask48;
    dvdend192[2]        = 0;
    dvdend192[3]        = 0;
    dvisor192[0]        = dvisor->leftPart & Mask48;
    dvisor192[1]        = dvisor->rightPart & Mask48;
    dvisor192[2]        = 0;
    dvisor192[3]        = 0;
    quotient->leftPart  = 0;
    quotient->rightPart = 0;
    i                   = 96;
    do
        {
        quotient->leftPart  = (quotient->leftPart << 1) | (quotient->rightPart >> 47);
        quotient->rightPart = (quotient->rightPart << 1) & Mask48;
        t[0]                = dvdend192[0];
        t[1]                = dvdend192[1];
        t[2]                = dvdend192[2];
        t[3]                = dvdend192[3];

        diff                = dvdend192[3] - dvisor192[3];
        borrow              = (diff >> 48) & 1;
        dvdend192[3]        = diff & Mask48;

        diff                = (dvdend192[2] - dvisor192[2]) - borrow;
        borrow              = (diff >> 48) & 1;
        dvdend192[2]        = diff & Mask48;

        diff                = (dvdend192[1] - dvisor192[1]) - borrow;
        borrow              = (diff >> 48) & 1;
        dvdend192[1]        = diff & Mask48;

        diff                = (dvdend192[0] - dvisor192[0]) - borrow;
        borrow              = (diff >> 48) & 1;
        dvdend192[0]        = diff & Mask48;

        if (borrow)
            {
            dvdend192[0] = t[0];
            dvdend192[1] = t[1];
            dvdend192[2] = t[2];
            dvdend192[3] = t[3];
            }
        else
            {
            quotient->rightPart |= 1;
            }

        dvisor192[3]   = (dvisor192[3] >> 1) | ((dvisor192[2] & 1) << 47);
        dvisor192[2]   = (dvisor192[2] >> 1) | ((dvisor192[1] & 1) << 47);
        dvisor192[1]   = (dvisor192[1] >> 1) | ((dvisor192[0] & 1) << 47);
        dvisor192[0] >>= 1;
        }
    while (--i >= 0);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform long multiplication of double precision coefficients
**
**  Parameters:     Name         Description.
**                  mltand       pointer to 96-bit multiplicand
**                  mltier       pointer to 96-bit multiplier
**                  hiProd       (out) pointer to high 96 bits of product
**                  loProd       (out) pointer to low  96 bits of product
**
**------------------------------------------------------------------------*/
static void float180LongMul(Cpu180Double *mltand, Cpu180Double *mltier, Cpu180Double *hiProd, Cpu180Double *loProd)
    {
    u64 m192[4];
    u64 p192[4];

    m192[0] = 0;
    m192[1] = 0;
    m192[2] = mltand->leftPart;
    m192[3] = mltand->rightPart;
    memset(p192, 0, sizeof(p192));
    while (mltier->leftPart != 0 || mltier->rightPart != 0)
        {
        //
        //  If the LSB of multiplier is 1, add multiplicand to product
        //
        if ((mltier->rightPart & 1) != 0)
            {
            p192[3] += m192[3];
            p192[2] += m192[2] + (p192[3] >> 48);
            p192[3] &= 0xffffffffffff;
            p192[1] += m192[1] + (p192[2] >> 48);
            p192[2] &= 0xffffffffffff;
            p192[0] += m192[0] + (p192[1] >> 48);
            p192[1] &= 0xffffffffffff;
            }
        //
        //  Left shift multiplicand (multiply by 2)
        //
        m192[0] = (m192[0] << 1) | (m192[1] >> 47);
        m192[1] = ((m192[1] << 1) | (m192[2] >> 47)) & 0xffffffffffff;
        m192[2] = ((m192[2] << 1) | (m192[3] >> 47)) & 0xffffffffffff;
        m192[3] = (m192[3] << 1) & 0xffffffffffff;
        //
        //  Right shift multiplier (divide by 2)
        //
        mltier->rightPart  = (mltier->rightPart >> 1) | ((mltier->leftPart & 1) << 47);
        mltier->leftPart >>= 1;
        }
    hiProd->leftPart  = p192[0];
    hiProd->rightPart = p192[1];
    loProd->leftPart  = p192[2];
    loProd->rightPart = p192[3];
    }

/*--------------------------------------------------------------------------
**  Purpose:        Normalize a double precision floating point exponent
**                  and coefficient
**
**  Parameters:     Name         Description.
**                  exponent     (in/out) biased exponent
**                  coefficient  (in/out) coefficient
**
**------------------------------------------------------------------------*/
static void float180NormalizeDouble(u16 *exponent, Cpu180Double *coefficient)
    {
    coefficient->rightPart &= Mask48;
    while ((coefficient->leftPart & 0xffff000000000000) != 0)
        {
        coefficient->rightPart  = (coefficient->rightPart >> 1) | ((coefficient->leftPart & 1) << 47);
        coefficient->leftPart >>= 1;
        *exponent              += 1;
        }
    if (coefficient->leftPart != 0 || coefficient->rightPart != 0)
        {
        while ((coefficient->leftPart & 0x800000000000) == 0)
            {
            coefficient->leftPart  = (coefficient->leftPart << 1) | (coefficient->rightPart >> 47);
            coefficient->rightPart = (coefficient->rightPart << 1) & Mask48;
            *exponent             -= 1;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Normalize a single precision floating point exponent
**                  and coefficient
**
**  Parameters:     Name         Description.
**                  exponent     (in/out) biased exponent
**                  coefficient  (in/out) coefficient
**
**------------------------------------------------------------------------*/
static void float180NormalizeFloat(u16 *exponent, u64 *coefficient)
    {
    while ((*coefficient & 0xffff000000000000) != 0)
        {
        *coefficient >>= 1;
        *exponent     += 1;
        }
    if ((*coefficient & 0x800000000000) == 0 && *coefficient != 0)
        {
        while ((*coefficient & 0x800000000000) == 0)
            {
            *coefficient <<= 1;
            *exponent     -= 1;
            }
        }
    }

/*---------------------------  End Of File  ------------------------------*/
