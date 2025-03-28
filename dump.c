/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: dump.c
**
**  Description:
**      Perform dump of PP and CPU memory as well as post-mortem
**      disassembly of PP memory.
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
static FILE *cpuF[2];
static FILE *ppuF[024];

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Initialize dumping.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpInit(void)
    {
    u8   cp;
    char fileName[20];
    u8   pp;

    for (cp = 0; cp < cpuCount; cp++)
        {
        sprintf(fileName, "cpu%o.dmp", cp);
        cpuF[cp] = fopen(fileName, "wt");
        if (cpuF[cp] == NULL)
            {
            logDtError(LogErrorLocation, "Can't open cpu[%o] dump", cp);
            }
        }

    for (pp = 0; pp < ppuCount; pp++)
        {
        sprintf(fileName, "ppu%02o.dmp", pp < 10 ? pp : (pp - 10) + 020);
        ppuF[pp] = fopen(fileName, "wt");
        if (ppuF[pp] == NULL)
            {
            logDtError(LogErrorLocation, "can't open ppu[%02o] dump", pp);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Terminate dumping.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpTerminate(void)
    {
    u8 cp;
    u8 pp;

    for (cp = 0; cp < cpuCount; cp++)
        {
        if (cpuF[cp] != NULL)
            {
            fclose(cpuF[cp]);
            }
        }

    for (pp = 0; pp < ppuCount; pp++)
        {
        if (ppuF[pp] != NULL)
            {
            fclose(ppuF[pp]);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Dump all PPs and CPU.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpAll(void)
    {
    u8 cp;
    u8 pp;

    logDtError(LogErrorLocation, "dumping core...");
    fflush(stderr);

    for (cp = 0; cp < cpuCount; cp++)
        {
        dumpCpu(cp);
        }

    for (pp = 0; pp < ppuCount; pp++)
        {
        dumpPpu(pp, 0, PpMemSize);
        dumpDisassemblePpu(pp);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Dump CPU.
**
**  Parameters:     Name        Description.
**                  cp          CPU number.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpCpu(u8 cp)
    {
    u32         addr;
    u8          ch;
    CpuContext *cpu;
    CpWord      data;
    bool        duplicateLine;
    u8          i;
    CpWord      lastData;
    FILE        *pf = cpuF[cp];
    u8          shiftCount;

    cpu = cpus + cp;
    fprintf(pf, "P       %06o  ", cpu->regP);
    fprintf(pf, "A%d %06o  ", 0, cpu->regA[0]);
    fprintf(pf, "B%d %06o", 0, cpu->regB[0]);
    fprintf(pf, "\n");

    fprintf(pf, "RA      %06o  ", cpu->regRaCm);
    fprintf(pf, "A%d %06o  ", 1, cpu->regA[1]);
    fprintf(pf, "B%d %06o", 1, cpu->regB[1]);
    fprintf(pf, "\n");

    fprintf(pf, "FL      %06o  ", cpu->regFlCm);
    fprintf(pf, "A%d %06o  ", 2, cpu->regA[2]);
    fprintf(pf, "B%d %06o", 2, cpu->regB[2]);
    fprintf(pf, "\n");

    fprintf(pf, "RAE   %08o  ", cpu->regRaEcs);
    fprintf(pf, "A%d %06o  ", 3, cpu->regA[3]);
    fprintf(pf, "B%d %06o", 3, cpu->regB[3]);
    fprintf(pf, "\n");

    fprintf(pf, "FLE   %08o  ", cpu->regFlEcs);
    fprintf(pf, "A%d %06o  ", 4, cpu->regA[4]);
    fprintf(pf, "B%d %06o", 4, cpu->regB[4]);
    fprintf(pf, "\n");

    fprintf(pf, "EM/FL %08o  ", cpu->exitMode);
    fprintf(pf, "A%d %06o  ", 5, cpu->regA[5]);
    fprintf(pf, "B%d %06o", 5, cpu->regB[5]);
    fprintf(pf, "\n");

    fprintf(pf, "MA      %06o  ", cpu->regMa);
    fprintf(pf, "A%d %06o  ", 6, cpu->regA[6]);
    fprintf(pf, "B%d %06o", 6, cpu->regB[6]);
    fprintf(pf, "\n");

    fprintf(pf, "ECOND       %02o  ", cpu->exitCondition);
    fprintf(pf, "A%d %06o  ", 7, cpu->regA[7]);
    fprintf(pf, "B%d %06o  ", 7, cpu->regB[7]);
    fprintf(pf, "\n");
    fprintf(pf, "STOP         %d  ", cpu->isStopped ? 1 : 0);
    fprintf(pf, "\n");
    fprintf(pf, "\n");

    for (i = 0; i < 8; i++)
        {
        fprintf(pf, "X%d ", i);
        data = cpu->regX[i];
        fprintf(pf, "%04o %04o %04o %04o %04o   ",
                (PpWord)((data >> 48) & Mask12),
                (PpWord)((data >> 36) & Mask12),
                (PpWord)((data >> 24) & Mask12),
                (PpWord)((data >> 12) & Mask12),
                (PpWord)((data) & Mask12));
        fprintf(pf, "\n");
        }

    fprintf(pf, "\n");

    lastData      = ~cpMem[0];
    duplicateLine = FALSE;
    for (addr = 0; addr < cpuMaxMemory; addr++)
        {
        data = cpMem[addr];

        if (data == lastData)
            {
            if (!duplicateLine)
                {
                fprintf(pf, "     DUPLICATED LINES.\n");
                duplicateLine = TRUE;
                }
            }
        else
            {
            duplicateLine = FALSE;
            lastData      = data;
            fprintf(pf, "%07o   ", addr & Mask21);
            fprintf(pf, "%04o %04o %04o %04o %04o   ",
                    (PpWord)((data >> 48) & Mask12),
                    (PpWord)((data >> 36) & Mask12),
                    (PpWord)((data >> 24) & Mask12),
                    (PpWord)((data >> 12) & Mask12),
                    (PpWord)((data) & Mask12));

            shiftCount = 60;
            for (i = 0; i < 10; i++)
                {
                shiftCount -= 6;
                ch          = (u8)((data >> shiftCount) & Mask6);
                fprintf(pf, "%c", cdcToAscii[ch]);
                }
            }

        if (!duplicateLine)
            {
            fprintf(pf, "\n");
            }
        }

    if (duplicateLine)
        {
        fprintf(pf, "LAST ADDRESS:%07o\n", addr & Mask21);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Dump PPU.
**
**  Parameters:     Name        Description.
**                  pp          PPU number.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpPpu(u8 pp, PpWord first, PpWord limit)
    {
    u32    addr;
    char   *fmt;
    u8     i;
    bool   is180;
    PpWord mask;
    PpWord *pm = ppu[pp].mem;
    FILE   *pf = ppuF[pp];
    PpWord pw;

    is180 = (features & IsCyber180) != 0;
    if (is180)
        {
        mask = Mask16;
        fmt  = "%06o %06o %06o %06o %06o %06o %06o %06o  ";
        }
    else
        {
        mask = Mask12;
        fmt  = "%04o %04o %04o %04o %04o %04o %04o %04o  ";
        }
    fprintf(pf, "P   %04o\n", ppu[pp].regP);
    fprintf(pf, "A %06o\n", ppu[pp].regA);
    if ((features & IsSeries800) != 0)
        {
        fprintf(pf, "R %010o\n", ppu[pp].regR);
        }
    if (is180 && ppu[pp].osBoundsCheckEnabled)
        {
        fprintf(pf, "OS bounds %s %010o\n", ppu[pp].isBelowOsBound ? "below" : "above", ppuOsBoundary);
        }
    if (ppu[pp].busy)
        {
        if (is180)
            {
            fprintf(pf, "PP busy: %04o%02o\n", ppu[pp].opF, ppu[pp].opD);
            }
        else
            {
            fprintf(pf, "PP busy: %02o%02o\n", ppu[pp].opF, ppu[pp].opD);
            }
        }
    fprintf(pf, "\n");

    for (addr = first; addr < limit; addr += 8)
        {
        fprintf(pf, "%04o  ", addr & Mask12);
        fprintf(pf, fmt,
                pm[addr + 0] & mask,
                pm[addr + 1] & mask,
                pm[addr + 2] & mask,
                pm[addr + 3] & mask,
                pm[addr + 4] & mask,
                pm[addr + 5] & mask,
                pm[addr + 6] & mask,
                pm[addr + 7] & mask);

        for (i = 0; i < 8; i++)
            {
            pw = pm[addr + i] & Mask12;
            fprintf(pf, "%c%c", cdcToAscii[(pw >> 6) & Mask6], cdcToAscii[pw & Mask6]);
            }

        fprintf(pf, "\n");
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Disassemble PPU memory.
**
**  Parameters:     Name        Description.
**                  pp          PPU number.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpDisassemblePpu(u8 pp)
    {
    u32    addr;
    u8     cnt;
    bool   is180;
    FILE   *pf;
    PpWord *pm = ppu[pp].mem;
    char   ppDisName[20];
    PpWord pw0, pw1;
    char   str[80];

    sprintf(ppDisName, "ppu%02o.dis", pp < 10 ? pp : (pp - 10) + 020);
    pf = fopen(ppDisName, "wt");
    if (pf == NULL)
        {
        logDtError(LogErrorLocation, "can't open %s", ppDisName);

        return;
        }

    ppuF[pp] = pf;
    dumpPpu(pp, 0, 0100);
    is180 = (features & IsCyber180) != 0;

    for (addr = 0100; addr < PpMemSize; addr += cnt)
        {
        fprintf(pf, "%04o  ", addr & Mask12);

        cnt = traceDisassembleOpcode(str, pm + addr);
        fputs(str, pf);

        if (is180)
            {
            pw0 = pm[addr] & Mask16;
            pw1 = pm[addr + 1] & Mask16;
            fprintf(pf, "   %06o ", pw0);
            if (cnt == 2)
                {
                fprintf(pf, "%06o  ", pw1);
                }
            else
                {
                fprintf(pf, "        ");
                }
            }
        else
            {
            pw0 = pm[addr] & Mask12;
            pw1 = pm[addr + 1] & Mask12;
            fprintf(pf, "   %04o ", pw0);
            if (cnt == 2)
                {
                fprintf(pf, "%04o  ", pw1);
                }
            else
                {
                fprintf(pf, "      ");
                }
            }

        fprintf(pf, "  %c%c", cdcToAscii[(pw0 >> 6) & Mask6], cdcToAscii[pw0 & Mask6]);

        if (cnt == 2)
            {
            fprintf(pf, "%c%c", cdcToAscii[(pw1 >> 6) & Mask6], cdcToAscii[pw1 & Mask6]);
            }

        fprintf(pf, "\n");
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Dump running PPU.
**
**  Parameters:     Name        Description.
**                  pp          PPU number.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpRunningPpu(u8 pp)
    {
    FILE *pf;
    char ppDumpName[20];

    sprintf(ppDumpName, "ppu%02o_run.dmp", pp < 10 ? pp : (pp - 10) + 020);
    pf = fopen(ppDumpName, "wt");
    if (pf == NULL)
        {
        logDtError(LogErrorLocation, "can't open %s", ppDumpName);

        return;
        }

    ppuF[pp] = pf;

    dumpPpu(pp, 0, PpMemSize);
    fclose(pf);

    ppuF[pp] = NULL;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Dump running CPU.
**
**  Parameters:     Name        Description.
**                  cp          CPU number.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpRunningCpu(u8 cp)
    {
    FILE *pf;
    char cpDumpName[20];

    sprintf(cpDumpName, "cpu%o_run.dmp", cp);
    pf = fopen(cpDumpName, "wt");
    if (pf == NULL)
        {
        logDtError(LogErrorLocation, "can't open %s", cpDumpName);

        return;
        }

    cpuF[cp] = pf;

    dumpCpu(cp);
    fclose(pf);

    cpuF[cp] = NULL;
    }

/*---------------------------  End Of File  ------------------------------*/
