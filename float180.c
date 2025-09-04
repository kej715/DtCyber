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
#define IsZero(exponent) (exponent < 0x3000)

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
static FloatClass float180FloatClassOf(u64 floatValue);
static void float180LongDiv(Cpu180Double *dvdend, Cpu180Double *dvisor, Cpu180Double *quotient);
static void float180LongMul(Cpu180Double *mltand, Cpu180Double *mltier, Cpu180Double *product);
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
    u16          shift;
    u8           signAddend;
    u8           signAugend;
    u8           signResult;

    classAddend = float180FloatClassOf(addend->leftPart);
    classAugend = float180FloatClassOf(augend->leftPart);
    switch ((classAugend * 5) + classAddend)
        {
    case Z1Z2xZ1Z2:
        sum->leftPart  = 0;
        sum->rightPart = 0;
        return TRUE;
    default:
    case Z1Z2xN:
    case NxN:
    case NxZ1Z2:
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
            if (shift < 96)
                {
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
            else
                {
                coeffAugend.leftPart  = 0;
                coeffAugend.rightPart = 0;
                }
            }
        else if (expAugend > expAddend)
            {
            expResult = expAugend;
            shift     = expAugend - expAddend;
            if (shift < 96)
                {
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
                coeffAddend.leftPart  = 0;
                coeffAddend.rightPart = 0;
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
        if (coeffResult.leftPart != 0 || coeffResult.rightPart != 0)
            {
            float180NormalizeDouble(&expResult, &coeffResult);
            sum->leftPart  = ((u64)signResult << 63) | ((u64)expResult << 48) | coeffResult.leftPart;
            sum->rightPart = ((u64)signResult << 63) | ((u64)expResult << 48) | coeffResult.rightPart;
            if (IsInfinite(expResult)) // exponent overflow
                {
                cpu180SetUserCondition(ctx, UCR58);
                if (IsUmrBitSet(ctx, UCR58) == FALSE)
                    {
                    sum->leftPart  = ((u64)signResult << 63) | INFINITE;
                    sum->rightPart = ((u64)signResult << 63) | INFINITE;
                    }
                }
            else if (IsZ2(expResult))  // exponent underflow
                {
                cpu180SetUserCondition(ctx, UCR59);
                if (IsUmrBitSet(ctx, UCR59) == FALSE)
                    {
                    sum->leftPart  = 0;
                    sum->rightPart = 0;
                    }
                }
            }
        else if (((augend->leftPart ^ addend->leftPart) == 0x8000000000000000) // N + -N
                 && (((augend->rightPart ^ addend->rightPart) & 0xffffffffffff) == 0))
            {
            sum->leftPart  = 0;
            sum->rightPart = 0;
            }
        else
            {
            cpu180SetUserCondition(ctx, UCR60); // FP loss of significance
            if (IsUmrBitSet(ctx, UCR60))
                {
                sum->leftPart  = (u64)expResult << 48; // Z3
                sum->rightPart = sum->leftPart;
                }
            else
                {
                sum->leftPart  = 0;
                sum->rightPart = 0;
                }
            }
        return TRUE;
    //
    //  See MIGDS 2-92 and 2-93
    //
    case Z1Z2xZ3:
    case Z3xZ1Z2:
    case Z3xZ3:
        cpu180SetUserCondition(ctx, UCR60); // FP loss of significance
        if (IsUmrBitSet(ctx, UCR59))
            {
            expAugend = ExponentOf(augend->leftPart);
            if (IsStandard(expAugend)) // if Z3
                {
                sum->leftPart  = (u64)expAugend << 48;
                sum->rightPart = sum->leftPart;
                }
            else
                {
                sum->leftPart  = ExponentOf(addend->leftPart) << 48;
                sum->rightPart = sum->leftPart;
                }
            }
        else
            {
            sum->leftPart  = 0;
            sum->rightPart = 0;
            }
        return TRUE;
    case Z3xN:
        expAddend             = ExponentOf(addend->leftPart);
        expAugend             = ExponentOf(augend->leftPart);
        coeffAddend.leftPart  = CoefficientOf(addend->leftPart);
        coeffAddend.rightPart = CoefficientOf(addend->rightPart);
        signAddend            = SignOf(addend->leftPart);
        if (expAddend >= expAugend)
            {
            *sum = *addend;
            }
        else
            {
            expResult = expAugend;
            shift     = expAugend - expAddend;
            if (shift < 96)
                {
                if (shift < 48)
                    {
                    coeffResult.rightPart  = (coeffAddend.rightPart >> shift) | ((coeffAddend.leftPart & coeffMasks[shift]) << (48 - shift));
                    coeffResult.leftPart   = coeffAddend.leftPart >> shift;
                    }
                else
                    {
                    coeffResult.rightPart = coeffAddend.leftPart >> (shift - 48);
                    coeffResult.leftPart  = 0;
                    }
                float180NormalizeDouble(&expResult, &coeffResult);
                sum->leftPart  = ((u64)signAddend << 63) | ((u64)expResult << 48) | coeffResult.leftPart;
                sum->rightPart = ((u64)signAddend << 63) | ((u64)expResult << 48) | coeffResult.rightPart;
                if (IsZ2(expResult))  // exponent underflow
                    {
                    cpu180SetUserCondition(ctx, UCR59);
                    if (IsUmrBitSet(ctx, UCR59) == FALSE)
                        {
                        sum->leftPart  = 0;
                        sum->rightPart = 0;
                        }
                    }
                }
            else
                {
                cpu180SetUserCondition(ctx, UCR60); // FP loss of significance
                if (IsUmrBitSet(ctx, UCR59))
                    {
                    sum->leftPart  = (u64)expResult << 48;
                    sum->rightPart = sum->leftPart;
                    }
                else
                    {
                    sum->leftPart  = 0;
                    sum->rightPart = 0;
                    }
                }
            }
        return TRUE;
    case NxZ3:
        expAddend             = ExponentOf(addend->leftPart);
        expAugend             = ExponentOf(augend->leftPart);
        coeffAugend.leftPart  = CoefficientOf(augend->leftPart);
        coeffAugend.rightPart = CoefficientOf(augend->rightPart);
        signAugend            = SignOf(augend->leftPart);
        if (expAugend >= expAddend)
            {
            *sum = *augend;
            }
        else
            {
            expResult = expAddend;
            shift     = expAddend - expAugend;
            if (shift < 96)
                {
                if (shift < 48)
                    {
                    coeffResult.rightPart  = (coeffAugend.rightPart >> shift) | ((coeffAugend.leftPart & coeffMasks[shift]) << (48 - shift));
                    coeffResult.leftPart   = coeffAugend.leftPart >> shift;
                    }
                else
                    {
                    coeffResult.rightPart = coeffAugend.leftPart >> (shift - 48);
                    coeffResult.leftPart  = 0;
                    }
                float180NormalizeDouble(&expResult, &coeffResult);
                sum->leftPart  = ((u64)signAugend << 63) | ((u64)expResult << 48) | coeffResult.leftPart;
                sum->rightPart = ((u64)signAugend << 63) | ((u64)expResult << 48) | coeffResult.rightPart;
                if (IsZ2(expResult))  // exponent underflow
                    {
                    cpu180SetUserCondition(ctx, UCR59);
                    if (IsUmrBitSet(ctx, UCR59) == FALSE)
                        {
                        sum->leftPart  = 0;
                        sum->rightPart = 0;
                        }
                    }
                }
            else
                {
                cpu180SetUserCondition(ctx, UCR60); // FP loss of significance
                if (IsUmrBitSet(ctx, UCR59))
                    {
                    sum->leftPart  = (u64)expResult << 48;
                    sum->rightPart = sum->leftPart;
                    }
                else
                    {
                    sum->leftPart  = 0;
                    sum->rightPart = 0;
                    }
                }
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
        sum->rightPart = sum->leftPart;
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
            sum->rightPart = sum->leftPart;
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
    u16        shift;
    u8         signAddend;
    u8         signAugend;
    u8         signResult;

    classAddend = float180FloatClassOf(addend);
    classAugend = float180FloatClassOf(augend);
    switch ((classAugend * 5) + classAddend)
        {
    case Z1Z2xZ1Z2:
        *sum = 0;
        return TRUE;
    default:
    case Z1Z2xN:
    case NxN:
    case NxZ1Z2:
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
            if (shift < 48)
                {
                coeffAugend >>= shift;
                }
            else
                {
                coeffAugend = 0;
                }
            }
        else if (expAugend > expAddend)
            {
            expResult = expAugend;
            shift     = expAugend - expAddend;
            if (shift < 48)
                {
                coeffAddend >>= shift;
                }
            else
                {
                coeffAddend = 0;
                }
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
        if (coeffResult != 0)
            {
            float180NormalizeFloat(&expResult, &coeffResult);
            *sum = ((u64)signResult << 63) | ((u64)expResult << 48) | coeffResult;
            if (IsInfinite(expResult)) // exponent overflow
                {
                cpu180SetUserCondition(ctx, UCR58);
                if (IsUmrBitSet(ctx, UCR58) == FALSE)
                    {
                    *sum = ((u64)signResult << 63) | INFINITE;
                    }
                }
            else if (IsZ2(expResult))  // exponent underflow
                {
                cpu180SetUserCondition(ctx, UCR59);
                if (IsUmrBitSet(ctx, UCR59) == FALSE)
                    {
                    *sum = 0;
                    }
                }
            }
        else if ((augend ^ addend) == 0x8000000000000000) // N + -N
            {
            *sum = 0;
            }
        else
            {
            cpu180SetUserCondition(ctx, UCR60); // FP loss of significance
            if (IsUmrBitSet(ctx, UCR60))
                {
                *sum = (u64)expResult << 48; // Z3
                }
            else
                {
                *sum = 0;
                }
            }
        return TRUE;
    //
    //  See MIGDS 2-92 and 2-93
    //
    case Z1Z2xZ3:
    case Z3xZ1Z2:
    case Z3xZ3:
        cpu180SetUserCondition(ctx, UCR60); // FP loss of significance
        if (IsUmrBitSet(ctx, UCR59))
            {
            expAugend = ExponentOf(augend);
            *sum = IsStandard(expAugend) ? (u64)expAugend << 48 : ExponentOf(addend) << 48;
            }
        else
            {
            *sum = 0;
            }
        return TRUE;
    case Z3xN:
        expAddend   = ExponentOf(addend);
        expAugend   = ExponentOf(augend);
        coeffAddend = CoefficientOf(addend);
        signAddend  = SignOf(addend);
        if (expAddend >= expAugend)
            {
            *sum = addend;
            }
        else
            {
            expResult = expAugend;
            shift     = expAugend - expAddend;
            if (shift < 48)
                {
                coeffResult = coeffAddend >> shift;
                float180NormalizeFloat(&expResult, &coeffResult);
                *sum = ((u64)signAddend << 63) | ((u64)expResult << 48) | coeffResult;
                if (IsZ2(expResult))  // exponent underflow
                    {
                    cpu180SetUserCondition(ctx, UCR59);
                    if (IsUmrBitSet(ctx, UCR59) == FALSE)
                        {
                        *sum = 0;
                        }
                    }
                }
            else
                {
                cpu180SetUserCondition(ctx, UCR60); // FP loss of significance
                if (IsUmrBitSet(ctx, UCR59))
                    {
                    *sum = (u64)expResult << 48;
                    }
                else
                    {
                    *sum = 0;
                    }
                }
            }
        return TRUE;
    case NxZ3:
        expAddend   = ExponentOf(addend);
        expAugend   = ExponentOf(augend);
        coeffAugend = CoefficientOf(addend);
        signAugend  = SignOf(augend);
        if (expAugend >= expAddend)
            {
            *sum = augend;
            }
        else
            {
            expResult = expAddend;
            shift     = expAddend - expAugend;
            if (shift < 48)
                {
                coeffResult = coeffAugend >> shift;
                float180NormalizeFloat(&expResult, &coeffResult);
                *sum = ((u64)signAugend << 63) | ((u64)expResult << 48) | coeffResult;
                if (IsZ2(expResult))  // exponent underflow
                    {
                    cpu180SetUserCondition(ctx, UCR59);
                    if (IsUmrBitSet(ctx, UCR59) == FALSE)
                        {
                        *sum = 0;
                        }
                    }
                }
            else
                {
                cpu180SetUserCondition(ctx, UCR60); // FP loss of significance
                if (IsUmrBitSet(ctx, UCR59))
                    {
                    *sum = (u64)expResult << 48;
                    }
                else
                    {
                    *sum = 0;
                    }
                }
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
    }

/*--------------------------------------------------------------------------
**  Purpose:        Compare two single precision floating point quantities
**                  and detect exceptions
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  minend      the minuend
**                  subend      the subtrahend
**                  valence     (out) -1 if minuend <  subend
**                                     0 if minuend == subend
**                                     1 if minuend >  subend
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
    FloatClass classSubend;
    FloatClass classMinend;
    u64        diff;
    u8         signSubend;
    u8         signMinend;

    classSubend = float180FloatClassOf(minend);
    classMinend = float180FloatClassOf(minend);
    switch ((classMinend * 5) + classSubend)
        {
    default:
    case NxN:
    case Z3xN:
    case NxZ3:
    case Z3xZ3:
        signMinend = SignOf(minend);
        signSubend = SignOf(minend);
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
        if (float180SubFloat(ctx, minend, subend, &diff))
            {
            if (diff == 0)
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
        *valence = (SignOf(minend) == 0) ? -1 : 1;
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
        signSubend = SignOf(minend);
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
    u16 exponent;
    u16 shift;

    exponent = ExponentOf(floatValue);

    if (IsIndefinite(exponent))
        {
        if (IsUcTrapEnabled(ctx, UCR61))
            {
            cpu180SetUserCondition(ctx, UCR61);
            return FALSE;
            }
        ctx->regUcr |= UcrBitMask(UCR61);
        *intResult   = 0;
        }
    else if (IsInfinite(exponent))
        {
        if (IsUcTrapEnabled(ctx, UCR62))
            {
            cpu180SetUserCondition(ctx, UCR62);
            return FALSE;
            }
        ctx->regUcr |= UcrBitMask(UCR62);
        *intResult   = 0;
        }
    else if (IsZero(floatValue) || exponent <= BIAS)
        {
        *intResult = 0;
        }
    else
        {
        *intResult = CoefficientOf(floatValue);
        shift      = exponent - BIAS;
        if (shift < 48)
            {
            *intResult >>= (48 - shift);
            }
        else if (shift > 48)
            {
            shift -= 48;
            if (shift < 15)
                {
                *intResult <<= shift;
                }
            else
                {
                if (IsUcTrapEnabled(ctx, UCR62))
                    {
                    cpu180SetUserCondition(ctx, UCR62);
                    return FALSE;
                    }
                ctx->regUcr |= UcrBitMask(UCR62);
                }
            }
        if (SignOf(floatValue) != 0)
            {
            *intResult = ~*intResult + 1;
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
    u16          expDvisor;
    u16          expDvdend;
    u16          expResult;
    u8           signDvisor;
    u8           signDvdend;
    u8           signResult;

    classDvisor = float180FloatClassOf(dvisor->leftPart);
    classDvdend = float180FloatClassOf(dvdend->leftPart);
    switch ((classDvdend * 5) + classDvisor)
        {
    default:
    case Z3xN:
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
            if ((coeffDvisor.leftPart << 1) <= coeffDvdend.leftPart) // divisor <= half of dividend
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
            coeffResult.leftPart  = (coeffResult.leftPart >> 1) | 0x800000000000;
            expResult            += 1;
            }
        float180NormalizeDouble(&expResult, &coeffResult);
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
        float180NormalizeFloat(&expResult, &coeffResult);
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
    Cpu180Double coeffResult;
    u16          expMltier;
    u16          expMltand;
    u16          expResult;
    u8           signMltier;
    u8           signMltand;
    u8           signResult;

    classMltier = float180FloatClassOf(mltier->leftPart);
    classMltand = float180FloatClassOf(mltand->leftPart);
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
        float180LongMul(&coeffMltand, &coeffMltier, &coeffResult);
        float180NormalizeDouble(&expResult, &coeffResult);
        product->leftPart  = ((u64)signResult << 63) | ((u64)expResult << 48) | coeffResult.leftPart;
        product->rightPart = ((u64)signResult << 63) | ((u64)expResult << 48) | coeffResult.rightPart;
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
    u8         signMltier;
    u8         signMltand;
    u8         signResult;
#if defined(_WIN32)
    u64        hiCoeff128;
    u64        loCoeff128;
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
        coeffResult = (hiCoeff128 << 16) | (loCoeff128 >> 48);
#else
        coeffResult = (u64)(((u128)coeffMltand * (u128)coeffMltier) >> 48);
#endif
        float180NormalizeFloat(&expResult, &coeffResult);
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
**  Purpose:        Determine the class of a floating point value
**
**  Parameters:     Name        Description.
**                  floatValue  the value for which to determine the class
**
**  Returns:        Floating point class.
**
**------------------------------------------------------------------------*/
static FloatClass float180FloatClassOf(u64 floatValue)
    {
    switch (floatValue >> 60)
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
        return (floatValue & Mask48) != 0 ? FloatClass_N : FloatClass_Z3;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform long division of double precision coefficients
**
**  Parameters:     Name         Description.
**                  dvdend       pointer to 192-bit normalized dividend
**                  dvisor       pointer to 96-bit normalized divisor
**                  quotient     pointer to quotient
**
**------------------------------------------------------------------------*/
static void float180LongDiv(Cpu180Double *dvdend, Cpu180Double *dvisor, Cpu180Double *quotient)
    {
    u8           i;
    u8           numBits;
    Cpu180Double remainder;
    Cpu180Double tmpDvisor;
    u64          packedQuotient[2];

    remainder         = *dvdend;
    tmpDvisor         = *dvisor;
    packedQuotient[0] = 0;
    packedQuotient[1] = 0;
    numBits           = 96;

    for (i = 0; i < numBits; i++)
        {
        //
        //  Shift divisor right
        //
        tmpDvisor.rightPart  = (tmpDvisor.rightPart >> 1) | ((tmpDvisor.leftPart & 1) << 47);
        tmpDvisor.leftPart >>= 1;
        //
        //  Shift quotient left to make space for new bit
        //
        packedQuotient[0]   = (packedQuotient[0] << 1) | (packedQuotient[1] >> 63);
        packedQuotient[1] <<= 1;
        //
        //  Subtract divisor from remainder, if possible
        //
        if (remainder.leftPart > tmpDvisor.leftPart
            || (remainder.leftPart == tmpDvisor.leftPart && remainder.rightPart >= tmpDvisor.rightPart))
            {
            remainder.rightPart -= tmpDvisor.rightPart;
            if (remainder.rightPart > 0x800000000000)
                {
                remainder.rightPart &= Mask48;
                remainder.leftPart  -= 1;
                }
            remainder.leftPart -= tmpDvisor.leftPart;
            packedQuotient[1] |= 1;
            }
        }
    quotient->rightPart = packedQuotient[1] & Mask48;
    quotient->leftPart  = (packedQuotient[0] << 16) | (packedQuotient[1] >> 48);
    if ((quotient->rightPart & 0xffffffff) == 0xffffffff) // round up if "noise" in least significant bits
        {
        quotient->rightPart += 1;
        quotient->leftPart  += quotient->rightPart >> 48;
        quotient->rightPart &= 0xffffffffffff;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform long multiplication of double precision coefficients
**
**  Parameters:     Name         Description.
**                  mltand       pointer to 96-bit normalized multiplicand
**                  mltier       pointer to 96-bit normalized multiplier
**                  product      pointer to product
**
**------------------------------------------------------------------------*/
static void float180LongMul(Cpu180Double *mltand, Cpu180Double *mltier, Cpu180Double *product)
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
    product->leftPart  = p192[0];
    product->rightPart = p192[1];
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
#if defined(_WIN32)
    u64 bit;

    if ((coefficient->leftPart & 0xffff000000000000) != 0)
        {
        while ((coefficient->leftPart & 0xffff000000000000) != 0)
            {
            bit                     = coefficient->leftPart & 1;
            coefficient->leftPart >>= 1;
            coefficient->rightPart  = (coefficient->rightPart >> 1) | (bit << 47);
            *exponent              += 1;
            }
        }
    else if (coefficient->leftPart != 0 || coefficient->rightPart != 0)
        {
        coefficient->rightPart &= 0xffffffffffff;
        while ((coefficient->leftPart & 0x800000000000) == 0)
            {
            coefficient->leftPart  = (coefficient->leftPart << 1) | (coefficient->rightPart >> 47);
            coefficient->rightPart = (coefficient->rightPart << 1) & 0xffffffffffff;
            *exponent     -= 1;
            }
        }
#else
    u128 coeff128;
    u128 mask;

    coeff128 = ((u128)coefficient->leftPart << 48) | (u128)coefficient->rightPart;
    mask     = (u128)0xffff << 96;
    if ((coeff128 & mask) != 0)
        {
        while ((coeff128 & mask) != 0)
            {
            coeff128 >>= 1;
            *exponent += 1;
            }
        }
    else if (coeff128 != 0)
        {
        mask = (u128)1 << 95;
        while ((coeff128 & mask) == 0)
            {
            coeff128 <<= 1;
            *exponent -= 1;
            }
        }
    coefficient->leftPart  = coeff128 >> 48;
    coefficient->rightPart = coeff128 & 0xffffffffffff;
#endif
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
    if ((*coefficient & 0xffff000000000000) != 0)
        {
        while ((*coefficient & 0xffff000000000000) != 0)
            {
            *coefficient >>= 1;
            *exponent     += 1;
            }
        }
    else if (*coefficient != 0)
        {
        while ((*coefficient & 0x800000000000) == 0)
            {
            *coefficient <<= 1;
            *exponent     -= 1;
            }
        }
    }

/*---------------------------  End Of File  ------------------------------*/
