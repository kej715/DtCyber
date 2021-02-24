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
static FILE *cpuF;
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
    u8 pp;
    char ppDumpName[20];

    cpuF = fopen("cpu.dmp", "wt");
    if (cpuF == NULL)
        {
        logError(LogErrorLocation, "can't open cpu dump");
        }

    for (pp = 0; pp < ppuCount; pp++)
        {
        sprintf(ppDumpName, "ppu%02o.dmp", pp);
        ppuF[pp] = fopen(ppDumpName, "wt");
        if (ppuF[pp] == NULL)
            {
            logError(LogErrorLocation, "can't open ppu[%02o] dump", pp);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Termiante dumping.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpTerminate(void)
    {
    u8 pp;

    if (cpuF != NULL)
        {
        fclose(cpuF);
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
    u8 pp;

    fprintf(stderr, "dumping core...");
    fflush(stderr);

    dumpCpu();
    for (pp = 0; pp < ppuCount; pp++)
        {
        dumpPpu(pp);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Dump CPU.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpCpu(void)
    {
    u32 addr;
    CpWord data;
    CpWord lastData;
    bool duplicateLine;
    u8 ch;
    u8 i;
    u8 shiftCount;

    fprintf(cpuF, "P       %06o  ", cpu.regP);
    fprintf(cpuF, "A%d %06o  ", 0, cpu.regA[0]);
    fprintf(cpuF, "B%d %06o", 0, cpu.regB[0]);
    fprintf(cpuF, "\n");
                           
    fprintf(cpuF, "RA      %06o  ", cpu.regRaCm);
    fprintf(cpuF, "A%d %06o  ", 1, cpu.regA[1]);
    fprintf(cpuF, "B%d %06o", 1, cpu.regB[1]);
    fprintf(cpuF, "\n");
                           
    fprintf(cpuF, "FL      %06o  ", cpu.regFlCm);
    fprintf(cpuF, "A%d %06o  ", 2, cpu.regA[2]);
    fprintf(cpuF, "B%d %06o", 2, cpu.regB[2]);
    fprintf(cpuF, "\n");
                           
    fprintf(cpuF, "RAE   %08o  ", cpu.regRaEcs);
    fprintf(cpuF, "A%d %06o  ", 3, cpu.regA[3]);
    fprintf(cpuF, "B%d %06o", 3, cpu.regB[3]);
    fprintf(cpuF, "\n");
                           
    fprintf(cpuF, "FLE   %08o  ", cpu.regFlEcs);
    fprintf(cpuF, "A%d %06o  ", 4, cpu.regA[4]);
    fprintf(cpuF, "B%d %06o", 4, cpu.regB[4]);
    fprintf(cpuF, "\n");
                           
    fprintf(cpuF, "EM/FL %08o  ", cpu.exitMode);
    fprintf(cpuF, "A%d %06o  ", 5, cpu.regA[5]);
    fprintf(cpuF, "B%d %06o", 5, cpu.regB[5]);
    fprintf(cpuF, "\n");
                           
    fprintf(cpuF, "MA      %06o  ", cpu.regMa);
    fprintf(cpuF, "A%d %06o  ", 6, cpu.regA[6]);
    fprintf(cpuF, "B%d %06o", 6, cpu.regB[6]);
    fprintf(cpuF, "\n");
                           
    fprintf(cpuF, "ECOND       %02o  ", cpu.exitCondition);
    fprintf(cpuF, "A%d %06o  ", 7, cpu.regA[7]);
    fprintf(cpuF, "B%d %06o  ", 7, cpu.regB[7]);
    fprintf(cpuF, "\n");
    fprintf(cpuF, "STOP         %d  ", cpuStopped ? 1 : 0);
    fprintf(cpuF, "\n");
    fprintf(cpuF, "\n");

    for (i = 0; i < 8; i++)
        {
        fprintf(cpuF, "X%d ", i);
        data = cpu.regX[i];
        fprintf(cpuF, "%04o %04o %04o %04o %04o   ",
            (PpWord)((data >> 48) & Mask12),
            (PpWord)((data >> 36) & Mask12),
            (PpWord)((data >> 24) & Mask12),
            (PpWord)((data >> 12) & Mask12),
            (PpWord)((data      ) & Mask12));
        fprintf(cpuF, "\n");
        }

    fprintf(cpuF, "\n");

    lastData = ~cpMem[0];
    duplicateLine = FALSE;
    for (addr = 0; addr < cpuMaxMemory; addr++)
        {
        data = cpMem[addr];

        if (data == lastData)
            {
            if (!duplicateLine)
                {
                fprintf(cpuF, "     DUPLICATED LINES.\n");
                duplicateLine = TRUE;
                }
            }
        else
            {
            duplicateLine = FALSE;
            lastData = data;
            fprintf(cpuF, "%07o   ", addr & Mask21);
            fprintf(cpuF, "%04o %04o %04o %04o %04o   ",
                (PpWord)((data >> 48) & Mask12),
                (PpWord)((data >> 36) & Mask12),
                (PpWord)((data >> 24) & Mask12),
                (PpWord)((data >> 12) & Mask12),
                (PpWord)((data      ) & Mask12));

            shiftCount = 60;
            for (i = 0; i < 10; i++)
                {
                shiftCount -= 6;
                ch = (u8)((data >> shiftCount) & Mask6);
                fprintf(cpuF, "%c", cdcToAscii[ch]);
                }
            }

        if (!duplicateLine) 
            {
            fprintf(cpuF, "\n");
            }
        }

    if (duplicateLine) 
        {
        fprintf(cpuF, "LAST ADDRESS:%07o\n", addr & Mask21);
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
void dumpPpu(u8 pp)
    {
    u32 addr;
    PpWord *pm = ppu[pp].mem;
    FILE *pf = ppuF[pp];
    u8 i;
    PpWord pw;

    fprintf(pf, "P   %04o\n", ppu[pp].regP);
    fprintf(pf, "A %06o\n", ppu[pp].regA);
    fprintf(pf, "R %08o\n", ppu[pp].regR);
    fprintf(pf, "\n");

    for (addr = 0; addr < PpMemSize; addr += 8)
        {
        fprintf(pf, "%04o   ", addr & Mask12);
        fprintf(pf, "%04o %04o %04o %04o %04o %04o %04o %04o  ",
            pm[addr + 0] & Mask12,
            pm[addr + 1] & Mask12,
            pm[addr + 2] & Mask12,
            pm[addr + 3] & Mask12,
            pm[addr + 4] & Mask12,
            pm[addr + 5] & Mask12,
            pm[addr + 6] & Mask12,
            pm[addr + 7] & Mask12);

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
    u32 addr;
    PpWord *pm = ppu[pp].mem;
    char ppDisName[20];
    FILE *pf;
    char str[80];
    u8 cnt;
    PpWord pw0, pw1;

    sprintf(ppDisName, "ppu%02o.dis", pp);
    pf = fopen(ppDisName, "wt");
    if (pf == NULL)
        {
        logError(LogErrorLocation, "can't open %s", ppDisName);
        return;
        }

    fprintf(pf, "P   %04o\n", ppu[pp].regP);
    fprintf(pf, "A %06o\n", ppu[pp].regA);
    fprintf(pf, "\n");

    for (addr = 0100; addr < PpMemSize; addr += cnt)
        {
        fprintf(pf, "%04o  ", addr & Mask12);

        cnt = traceDisassembleOpcode(str, pm + addr);
        fputs(str, pf);

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

    sprintf(ppDumpName, "ppu%02o_run.dmp", pp);
    pf = fopen(ppDumpName, "wt");
    if (pf == NULL)
        {
        logError(LogErrorLocation, "can't open %s", ppDumpName);
        return;
        }

    ppuF[pp] = pf;

    dumpPpu(pp);
    fclose(pf);

    ppuF[pp] = NULL;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Dump running CPU.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpRunningCpu(void)
    {
    cpuF = fopen("cpu_run.dmp", "wt");
    if (cpuF == NULL)
        {
        logError(LogErrorLocation, "can't open cpu_run.dmp");
        return;
        }

    dumpCpu();
    fclose(cpuF);

    cpuF = NULL;
    }

/*---------------------------  End Of File  ------------------------------*/
