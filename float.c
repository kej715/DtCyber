/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Steve Peltz, Tom Hunter
**
**  Name: float.c
**
**  Description:
**      Perform CDC 6600 floating point unit functions.
**
**  Note:
**      Lot of this source file has been "borrowed" from a floating point
**      implementation done by Steve Peltz for his Zephyr emulator.
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

/*
**  -------------
**  Include Files
**  -------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "const.h"
#include "types.h"
#include "proto.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define ID  ((CpWord)01777)
#define OR  ((CpWord)03777)

#define IR(x)   (((x) & ID) != ID)
#define OVFL(s) ((OR ^ (s >> 48)) << 48)

#define IND (ID << 48)

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/

/*
**  The following code fragments are used throughout:
**
**  sign = SignX(v, 60);                    extract sign bit
**  v ^= sign;                              make absolute
**  exponent = (v >> 48);                   extract exponent
**                                          
**  exponent -= 02000;                      un-bias exponent
**  exponent -= (exponent >> 11);           make two's complement
**
**  exponent += 02000 + (exponent >> 11);   make one's comp
**
**  (if exponent is still less than zero (two's complement) then
**  it is an underflow condition)
*/

#define SignX(v, bit) (((v) & ((CpWord)1 << ((bit) - 1))) == 0 ? 0 : Mask60)

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

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*
**  Visual C++ 6.0 Enterprise Edition appears to have a bug in its optimizer
**  which results in incorrect maths. The line affected by this bug is marked
**  with a comment "line triggers Visual C++ OPTIMIZER BUG". To avoid the problem
**  optimization is turned off for the whole file.
*/
#if defined(_WIN32)
#pragma optimize("", off)
#endif

/*--------------------------------------------------------------------------
**  Purpose:        Floating point addition.
**                  Extract both signs, make absolute, extract exponents,
**                  check for special cases, shift smaller exponent to the right,
**                  do a 96 bit signed one's complement add, shift right by one
**                  if overflow.
**                  Return upper 48 bits and adjusted exponent for 
**
**  Parameters:     Name        Description.
**                  v1          First operand
**                  v2          Second operand
**                  doRound     TRUE if rounding required, FALSE otherwise.
**                  doDouble    TRUE if double precision required, FALSE otherwise.
**
**  Returns:        Upper 48 bits and adjusted exponent for single precision.
**                  Lower 48 bits and adjusted exponent for double precision.
**
**------------------------------------------------------------------------*/
CpWord floatAdd(CpWord v1, CpWord v2, bool doRound, bool doDouble)
    {
    CpWord   sign1;
    CpWord   sign2;
    int exponent1;
    int exponent2;
    int shift;
    CpWord   upper;  /* upper half of 98 bit adder register - 50 bits */
    CpWord   lower;  /* bottom half is 48 bits */
    CpWord   round = 0;
    CpWord   result;

    sign1 = SignX(v1, 60);
    sign2 = SignX(v2, 60);

    v1 ^= sign1;
    v2 ^= sign2;

    exponent1 = (int)(v1 >> 48);
    exponent2 = (int)(v2 >> 48);

    if (!IR(exponent1))
        {
        if ((exponent1 == ID) || (exponent2 == ID) || ((exponent2 == OR) && (sign1 != sign2)))
            {
            return IND;
            }

        return OVFL(sign1);
        }

    if (!IR(exponent2))
        {
        if (exponent2 == ID)
            {
            return IND;
            }

        return OVFL(sign2);
        }

    exponent1 -= 02000;
    exponent2 -= 02000;

    exponent1 -= (exponent1 >> 11);
    exponent2 -= (exponent2 >> 11);

    /*
    **  pre-set round bit - second rounding bit is inserted if
    **  both values are normalized or the signs are different.
    **  bit 47 is the rounding bit. calculate this part here
    **  even though it won't be needed if shift is 48 or more,
    **  so the variables can be over-used.
    **
    **  if both variables are normalized (both have bit 47 set), then
    **  the round bit will be set. if the signs are different, all 60
    **  bits will be set, so the value must be masked later on if it is
    **  used.
    */
    if (doRound)
        {
        round = (v1 & v2) | (sign1 ^ sign2);
        }

    sign1 >>= 10;
    sign2 >>= 10;

    /*
    **  move value with larger exponent to 98 bit "register", and
    **  leave smaller value in v1, larger exponent in exponent1,
    **  and calculate shift count. note that the lower 48 bits of
    **  the first value are extended with the sign.
    **
    **  value with larger exponent always has a rounding bit
    **  inserted after the least significant bit (top bit of
    **  lower part of register).
    */
    if (exponent1 > exponent2)
        {
        upper = (v1 & Mask48) ^ sign1;
        v1 = v2 & Mask48;
        lower = sign1 >> 2;
        if (doRound)
            {
            lower = Sign48 ^ lower;  /* rounding bit */
            }
        sign1 = sign2;
        shift = exponent1 - exponent2;
        }
    else
        {
        upper = (v2 & Mask48) ^ sign2;
        v1 &= Mask48;
        lower = sign2 >> 2;
        if (doRound)
            {
            lower = Sign48 ^ lower;  /* rounding bit */
            }
        shift = exponent2 - exponent1;
        exponent1 = exponent2;
        }

    /*
    **  sign1 is 50 bit sign extension of second (smaller exponent) value;
    **  make sign2 be the 48 bit version for the lower half.
    */
    sign2 = sign1 >> 2;

    /*
    **  three possible choices - if shift count is less than 48 bits,
    **  add to both upper and lower (or just upper if shift count is
    **  zero, but that gets taken care of automatically); if shift count
    **  is 48 through 95, add to just the lower half; else just add in
    **  sign bits (result is shifted off the end of the register).
    **
    **  if shift is less than 48, insert shifted rounding bit.
    */
    if (shift < 48)
        {
        upper += (v1 >> shift) ^ sign1;
        if (doRound)
            {
            round = (round & Sign48) >> shift;
            lower += (((v1 << (48 - shift)) & Mask48) | round) ^ sign2;
            }
        else
            {
            lower += ((v1 << (48 - shift)) & Mask48) ^ sign2;
            }
        }
    else if (shift < 96)
        {
        upper += sign1;
        lower += (v1 >> (shift - 48)) ^ sign2;
        }
    else
        {
        upper += sign1;
        lower += sign2;
        }

    /*
    **  carry-out from lower to upper, mask off overflow, add one if
    **  adding one would cause an end-around carry, then carry-out
    **  again from lower to upper. this is the same algorithm as the
    **  18 or 60 bit one's complement add uses, and adjusts for -0
    **  the same way the cyber does (cyber actually inverts the second
    **  operand and subtracts, rather than adds)
    */
    upper += lower >> 48;
    lower &= Mask48;
    lower += (upper + ((lower + 1) >> 48)) >> 50;   // line triggers Visual C++ OPTIMIZER BUG
    upper += lower >> 48;
    upper &= Mask50;
    lower &= Mask48;

    /*
    **  get sign of result, and make result absolute
    */
    sign1 = SignX(upper, 50);
    upper ^= (sign1 >> 10);
    lower ^= (sign1 >> 12);

    if (doDouble)
        {
        if ((features & Has175Float) != 0)
            {
            /*
            **  check if exponent will be out of range after subtracting 48 (if
            **  resulting exponent is less than -1777 (octal), a positive zero is
            **  returned).
            */
            if (exponent1 < -01717)
                {
                return(0);
                }
            }

        /*
        **  post-normalize - shift bottom bit of upper half to upper
        **  bit of bottom half (instead of shifting the upper half, as
        **  the other add routines do)
        */
        if (upper >> 48)
            {
            lower = ((upper & 1) << 47) | (lower >> 1);
            exponent1 += 1;
            }

        /*
        **  check if exponent will be out of range after subtracting 48 (if
        **  resulting exponent is -1777 (octal), the result is an underflow,
        **  but the significant digits are still returned; if it is less than
        **  that, a positive zero is returned). -1777 is 0000 in one's complement
        **  biased form.
        */
        if (exponent1 < -01717)
            {
            return(0);
            }

        exponent1 -= 48;
        exponent1 += 02000 + (exponent1 >> 11);
        result =  ((((CpWord) exponent1) << 48) | (lower & Mask48)) ^ sign1;
        }
    else
        {
        /*
        **  post-normalize if necessary
        */
        if (upper >> 48)
            {
            upper >>= 1;
            exponent1 += 1;
            }

        exponent1 += 02000 + (exponent1 >> 11);

        result = ((((CpWord) exponent1) << 48) | upper) ^ sign1;
        }

    return(result);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Floating multiply
**                  do four 24 bit multiplies, combine, offset exponent and return
**                  upper 48 bits of result. does a one bit post-normalize if
**                  necessary if both values were normalized to begin with.
**                  
**                  Double precision multiply. returns the bottom half of the 96
**                  bit product. also used for integer multiply by checking for
**                  both exponents zero and one or both values not normalized.
**                  
**                  Rounding multiply is almost identical to floating multiply,
**                  except that a single rounding bit is added to the result in
**                  bit 46 of the 96 bit product.
**
**  Parameters:     Name        Description.
**                  v1          First operand
**                  v2          Second operand
**                  doRound     TRUE if rounding required, FALSE otherwise.
**                  doDouble    TRUE if double precision required, FALSE otherwise.
**
**  Returns:        Upper 48 bits and adjusted exponent for single precision.
**                  Lower 48 bits and adjusted exponent for double precision.
**
**------------------------------------------------------------------------*/
CpWord floatMultiply(CpWord v1, CpWord v2, bool doRound, bool doDouble)
    {
    CpWord  sign1;
    CpWord  sign2;
    int exponent1;
    int exponent2;
    int norm;           /* flag for post-normalize */
    CpWord  upper;      /* upper 48 bits of product */
    CpWord  middle;     /* middle cross-product */
    CpWord  lower;      /* lower 48 bits of product */

    sign1 = SignX(v1, 60);
    sign2 = SignX(v2, 60);

    v1 ^= sign1;
    v2 ^= sign2;
    /*
    **  get sign of result
    */
    sign1 ^= sign2;

    exponent1 = (int)(v1 >> 48);
    exponent2 = (int)(v2 >> 48);

    if (!IR(exponent1))
        {
        if ((exponent1 == ID) || (exponent2 == ID) || !exponent2)
            {
            return IND;
            }

        return OVFL(sign1);
        }

    if (!IR(exponent2))
        {
        if ((exponent2 == ID) || !exponent1)
            {
            return IND;
            }

        return OVFL(sign1);
        }

    v1 &= Mask48;
    v2 &= Mask48;

    /*
    **  get the post-normalize flag
    */
    norm = (int)((v1 & v2) >> 47);

    /*
    **  form middle cross-product, upper and lower product, and add them
    **  all together, with a carry from lower to upper.
    */
    middle = (v1 & Mask24) * (v2 >> 24);
    if (doRound)
        {
        /*
        **  rounding bit (46) is bit 22 in the middle cross-product.
        */
        middle += ((CpWord)1 << 22);
        }

    middle += (v1 >> 24) * (v2 & Mask24);
    lower = (v1 & Mask24) * (v2 & Mask24);
    lower += (middle & Mask24) << 24;
    upper = (v1 >> 24) * (v2 >> 24);
    upper += (middle >> 24) + (lower >> 48);

    /*
    **  do an integer multiply if one or both values are not normalized
    **  and both exponents are zero (this is really only specified for
    **  the double precision multiply, but the same check is done for
    **  floating and rounding as well - this is necessary to make -0
    **  results come out correctly.
    */
    if (doDouble)
        {
        if (!(norm | exponent1 | exponent2))
            {
            return (lower & Mask48) ^ sign1;
            }

        if (!(exponent1 && exponent2))
            {
            return 0;
            }

        upper = (v1 >> 24) * (v2 >> 24);
        upper += (middle >> 24) + (lower >> 48);

        exponent1 -= 02000;
        exponent2 -= 02000;

        exponent1 -= (exponent1 >> 11);
        exponent2 -= (exponent2 >> 11);

        exponent1 += exponent2;

        if ((features & Has175Float) != 0)
            {
            if (exponent1 > 01777)
                {
                return OVFL(sign1);
                }

            if (exponent1 <= -01777)
                {
                return(0);
                }
            }

        if (norm && !(upper >> 47))
            {
            lower <<= 1;
            exponent1 -= 1;
            }

        /*
        **  since the bottom half is returned, exponent doesn't need to be
        **  offset by 48.
        */
        if (exponent1 > 01777)
            {
            return OVFL(sign1);
            }

        exponent1 += 02000 + (exponent1 >> 11);

        if (exponent1 < 0)
            {
            return 0;
            }

        return ((((CpWord) exponent1) << 48) | (lower & Mask48)) ^ sign1;
        }


    if (!(norm | exponent1 | exponent2))
        {
        return upper ^ sign1;
        }

    /*
    **  if not an integer multiply and one or both exponents are zero
    **  (underflow), return positive zero.
    */
    if (!(exponent1 && exponent2))
        return 0;

    exponent1 -= 02000;
    exponent2 -= 02000;

    exponent1 -= (exponent1 >> 11);
    exponent2 -= (exponent2 >> 11);

    exponent1 += exponent2; /* add exponents together for multiply */

    if ((features & Has175Float) != 0)
        {
        if ((exponent1 + 48) > 01777)
            {
            return OVFL(sign1);
            }

        if ((exponent1 + 48) <= -01777)
            {
            return(0);
            }
        }

    /*
    **  post normalize if necessary
    */
    if (norm && !(upper >> 47))
        {
        upper = (upper << 1) | ((lower >> 47) & 1);
        exponent1 -= 1;
        }

    /*
    **  offset exponent by 48, since we ended up with a 96 bit product
    **  and are only returning the upper 48 bits. check for overflow
    **  values (biased values less than 0000 or greater than 3777).
    */
    if (exponent1 > 01717)
        {
        return OVFL(sign1);
        }

    exponent1 += 48;
    exponent1 += 02000 + (exponent1 >> 11);

    if (exponent1 < 0)
        {
        return 0;
        }

    return ((((CpWord) exponent1) << 48) | upper) ^ sign1;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Floating divide implemented using shift and subtract.
**
**                  Rounding divide is identical to floating divide, except
**                  that as the dividend gets shifted in, 1/3 is shifted in
**                  (1/3 is alternating bits: 25252525... octal).
**
**  Parameters:     Name        Description.
**                  v1          First operand
**                  v2          Second operand
**                  doRound     TRUE if rounding required, FALSE otherwise.
**
**  Returns:        Upper 48 bits and adjusted exponent for single precision.
**                  Lower 48 bits and adjusted exponent for double precision.
**
**------------------------------------------------------------------------*/
CpWord floatDivide(CpWord v1, CpWord v2, bool doRound)
    {
    CpWord  sign1;
    CpWord  sign2;
    int round = 0;
    int exponent1;
    int exponent2;

    sign1 = SignX(v1, 60);
    sign2 = SignX(v2, 60);

    v1 ^= sign1;
    v2 ^= sign2;

    exponent1 = (int)(v1 >> 48);
    exponent2 = (int)(v2 >> 48);

    sign1 ^= sign2;

    /*
    **  indefinite divided by anything is indefinite
    **  anything divided by indefinite is indefinite
    **  infinite divided by infinite is indefinite
    **  infinite divided by anything else is infinite
    */
    if (!IR(exponent1))
        {
        if ((exponent1 == ID) || (exponent2 == ID) || (exponent2 == OR))
            {
            return IND;
            }

        return OVFL(sign1);
        }

    if (!IR(exponent2))
        {
        if (exponent2 == ID)
            {
            return IND;
            }

        return 0;
        }

    /*
    **  exponent = 0 is taken to mean value = 0
    **  if non-zero divided by zero, return overflow
    **  if zero divided by non-zero, return positive zero
    **  if zero divided by zero, return positive indefinite
    */
    if (!(exponent1 && exponent2))
        {
        if (exponent1)
            {
            return OVFL(sign1);
            }

        if (exponent2)
            {
            return 0;
            }

        return IND;
        }

    v1 &= Mask48;
    v2 &= Mask48;

    /*
    **  if divisor is less than half of dividend, return indefinite - divisor
    **  should be normalized, but it isn't checked for explicitly.
    */
    if (v1 >= (v2 << 1))
        return IND;

    exponent1 -= 02000;
    exponent2 -= 02000;

    exponent1 -= (exponent1 >> 11);
    exponent2 -= (exponent2 >> 11);

    /*
    **  divide exponents by subtracting
    */
    exponent1 -= exponent2;

    /*
    **  pre-normalize if necessary. this is guaranteed to make v1 >= v2
    **  due to earlier check.
    */
    if (v1 < v2)
        {
        v1 <<= 1;
        exponent1 -= 1;
        if (doRound)
            {
            round = 1;  /* round bit (of zero) got shifted in */
            }
        }

    /*
    **  figure out final exponent and check for overflow before
    **  actually doing the divides
    */
    if (exponent1 > 02056)  /* 1777 + 0057 (octal) */
        {
        return OVFL(sign1);
        }

    exponent1 -= 47;
    exponent1 += 02000 + (exponent1 >> 11);

    if (exponent1 < 0)
        {
        return 0;
        }

    sign2 = 0;  /* used to accumulate the result */

    /*
    **  main divide loop - shift and subtract for 48 bits
    */
    for (exponent2 = 47; exponent2 >= 0; exponent2--)
        {
        sign2 <<= 1;
        if (v1 >= v2)
            {
            v1 -= v2;
            sign2 += 1;
            }

        if (doRound)
            {
            v1 = (v1 << 1) | round; /* shift in rounding bit */
            round = 1 - round;      /* toggle round back and forth */
            }
        else
            {
            v1 <<= 1;
            }
        }

    return ((((CpWord) exponent1) << 48) | sign2) ^ sign1;
    }

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*---------------------------  End Of File  ------------------------------*/
