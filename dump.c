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
static void dumpPrintPva(FILE *pf, u64 pva);

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
    u8   cp;
    char fileName[20];
    u8   pp;

    for (cp = 0; cp < cpuCount; cp++)
        {
        strcpy(fileName, "cpu.dmp");
        cpuF = fopen(fileName, "wt");
        if (cpuF == NULL)
            {
            logDtError(LogErrorLocation, "Can't open cpu dump");
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
        if (cpuF != NULL)
            {
            fclose(cpuF);
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
    u8 pp;

    logDtError(LogErrorLocation, "dumping core...");
    fflush(stderr);

    dumpCpu();

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
void dumpCpu(void)
    {
    u32           addr;
    u8            ch;
    u8            cp;
    Cpu170Context *cpu170;
    Cpu180Context *cpu180;
    CpWord        data;
    bool          duplicateLine;
    u8            i;
    CpWord        lastData;
    FILE          *pf = cpuF;
    u8            shiftCount;

    for (cp = 0; cp < cpuCount; cp++)
        {
        fprintf(pf, "[CPU%d", cp);
        if (isCyber180)
            {
            fputs(" : Cyber 170 state", pf);
            }
        fputs("]\n", pf);
        cpu170 = cpus170 + cp;
        fprintf(pf, "P       %06o  ", cpu170->regP);
        fprintf(pf, "A%d %06o  ", 0, cpu170->regA[0]);
        fprintf(pf, "B%d %06o", 0, cpu170->regB[0]);
        fputs("\n", pf);

        fprintf(pf, "RA      %06o  ", cpu170->regRaCm);
        fprintf(pf, "A%d %06o  ", 1, cpu170->regA[1]);
        fprintf(pf, "B%d %06o", 1, cpu170->regB[1]);
        fputs("\n", pf);

        fprintf(pf, "FL      %06o  ", cpu170->regFlCm);
        fprintf(pf, "A%d %06o  ", 2, cpu170->regA[2]);
        fprintf(pf, "B%d %06o", 2, cpu170->regB[2]);
        fputs("\n", pf);

        fprintf(pf, "RAE   %08o  ", cpu170->regRaEcs);
        fprintf(pf, "A%d %06o  ", 3, cpu170->regA[3]);
        fprintf(pf, "B%d %06o", 3, cpu170->regB[3]);
        fputs("\n", pf);

        fprintf(pf, "FLE   %08o  ", cpu170->regFlEcs);
        fprintf(pf, "A%d %06o  ", 4, cpu170->regA[4]);
        fprintf(pf, "B%d %06o", 4, cpu170->regB[4]);
        fputs("\n", pf);

        fprintf(pf, "EM/FL %08o  ", cpu170->exitMode);
        fprintf(pf, "A%d %06o  ", 5, cpu170->regA[5]);
        fprintf(pf, "B%d %06o", 5, cpu170->regB[5]);
        fputs("\n", pf);

        fprintf(pf, "MA      %06o  ", cpu170->regMa);
        fprintf(pf, "A%d %06o  ", 6, cpu170->regA[6]);
        fprintf(pf, "B%d %06o", 6, cpu170->regB[6]);
        fputs("\n", pf);

        fprintf(pf, "ECOND       %02o  ", cpu170->exitCondition);
        fprintf(pf, "A%d %06o  ", 7, cpu170->regA[7]);
        fprintf(pf, "B%d %06o  ", 7, cpu170->regB[7]);
        fputs("\n", pf);
        fprintf(pf, "STOP         %d  ", cpu170->isStopped ? 1 : 0);
        fputs("\n\n", pf);

        for (i = 0; i < 8; i++)
            {
            fprintf(pf, "X%d ", i);
            data = cpu170->regX[i];
            fprintf(pf, "%04o %04o %04o %04o %04o",
                    (PpWord)((data >> 48) & Mask12),
                    (PpWord)((data >> 36) & Mask12),
                    (PpWord)((data >> 24) & Mask12),
                    (PpWord)((data >> 12) & Mask12),
                    (PpWord)((data) & Mask12));
            fputs("\n", pf);
            }
        fputs("\n", pf);

        if (isCyber180)
            {
            fprintf(pf, "[CPU%d : Cyber 180 state]\n", cp);
            cpu180 = cpus180 + cp;
            data = cpu180->regX[i];
            fprintf(pf, " P %02x ", (PpWord)((data >> 48) & Mask8)); // key
            dumpPrintPva(pf, data);
            fputs("\n\n", pf);
            for (i = 0; i < 16; i++)
                {
                data = cpu180->regX[i];
                fprintf(pf, "A%X ", i);
                dumpPrintPva(pf, cpu180->regA[i]);
                fprintf(pf,"   X%X %04x %04x %04x %04x\n", i,
                        (PpWord)((data >> 48) & Mask16),
                        (PpWord)((data >> 32) & Mask16),
                        (PpWord)((data >> 16) & Mask16),
                        (PpWord)((data) & Mask16));
                }
            fputs("\n", pf);
            fprintf(pf, "VMID %04x LPID %02x\n", cpu180->regVmid, cpu180->regLpid);
            fprintf(pf, " UMR %04x  MMR %04x         Flags %02x\n", cpu180->regUmr, cpu180->regMmr, cpu180->regFlags);
            fprintf(pf, " UCR %04x  MCR %04x  Trap Enables %02x\n", cpu180->regUcr, cpu180->regMcr, cpu180->regTe);
            fprintf(pf, "                              MDF %04x\n", cpu180->regMdf);
            fputs("\n", pf);
            fprintf(pf, " MPS %08x   BC %08x\n", cpu180->regMps, cpu180->regBc);
            fprintf(pf, " JPS %08x  PIT %08x\n", cpu180->regJps, cpu180->regPit);
            fputs("\n", pf);
            fprintf(pf, " PTA %08x  STA %08x\n", cpu180->regPta, cpu180->regSta);
            fprintf(pf, " PTL %02x        STL %04x\n", cpu180->regPtl, cpu180->regStl);
            fprintf(pf, " PSM %02x\n", cpu180->regPsm);
            fputs("\n", pf);
            fputs(" UTP ", pf);
            dumpPrintPva(pf, cpu180->regUtp);
            fputs("   TP ", pf);
            dumpPrintPva(pf, cpu180->regTp);
            fputs("\n", pf);
            fputs(" DLP ", pf);
            dumpPrintPva(pf, cpu180->regDlp);
            fprintf(pf, "   DI %02x\n", cpu180->regDi);
            fprintf(pf, "                      DM %02x\n", cpu180->regDm);
            fputs("\n", pf);
            fprintf(pf, " LRN %d\n", cpu180->regLrn);
            for (i = 0; i < 15; i++)
                {
                fprintf(pf, " TOS[%02d] ", i + 1);
                dumpPrintPva(pf, cpu180->regTos[i]);
                fputs("\n", pf);
                }
            }
            fputs("\n", pf);
            fprintf(pf, " MDW %016lx  \n", cpu180->regMdw);
            fputs("\n", pf);
        }

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
            if (isCyber180)
                {
                fprintf(pf, "%08o  ", addr & Mask24);
                }
            else
                {
                fprintf(pf, "%07o   ", addr & Mask21);
                }
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
            if (isCyber180)
                {
                fprintf(pf, "    %08x  %04x %04x %04x %04x   ",
                        (addr << 3) & Mask24,
                        (PpWord)((data >> 48) & Mask16),
                        (PpWord)((data >> 32) & Mask16),
                        (PpWord)((data >> 16) & Mask16),
                        (PpWord)((data) & Mask16));
                shiftCount = 64;
                for (i = 0; i < 8; i++)
                    {
                    shiftCount -= 8;
                    ch          = (u8)((data >> shiftCount) & Mask8);
                    fprintf(pf, "%c", (ch > 0x1f && ch < 0x7f) ? ch : '.');
                    }
                }
            }

        if (!duplicateLine)
            {
            fprintf(pf, "\n");
            }
        }

    if (duplicateLine)
        {
        if (isCyber180)
            {
            fprintf(pf, "LAST ADDRESS:%08o\n", addr & Mask24);
            }
        else
            {
            fprintf(pf, "LAST ADDRESS:%07o\n", addr & Mask21);
            }
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
    PpWord mask;
    PpWord *pm = ppu[pp].mem;
    FILE   *pf = ppuF[pp];
    PpWord pw;

    if (isCyber180)
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
    if (isCyber180 && ppu[pp].osBoundsCheckEnabled)
        {
        fprintf(pf, "OS bounds %s %010o\n", ppu[pp].isBelowOsBound ? "below" : "above", ppuOsBoundary);
        }
    if (ppu[pp].busy)
        {
        if (isCyber180)
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
**  Purpose:        Print a Cyber 180 Process Virtual Address.
**
**  Parameters:     Name        Description.
**                  pf          pointer to file descriptor
**                  pva         the PVA to print
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void dumpPrintPva(FILE *pf, u64 pva)
    {
    fprintf(pf, "%x %03x %08x",
            (PpWord)((pva >> 44) & Mask4),   // ring
            (PpWord)((pva >> 32) & Mask12),  // segment
            (PpWord)(pva & Mask32));         // byte number
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

    for (addr = 0100; addr < PpMemSize; addr += cnt)
        {
        fprintf(pf, "%04o  ", addr & Mask12);

        cnt = traceDisassembleOpcode(str, pm + addr);
        fputs(str, pf);

        if (isCyber180)
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
void dumpRunningCpu(void)
    {
    FILE *pf;
    char *cpDumpName = "cpu_run.dmp";

    pf = fopen(cpDumpName, "wt");
    if (pf == NULL)
        {
        logDtError(LogErrorLocation, "can't open %s", cpDumpName);

        return;
        }

    cpuF = pf;

    dumpCpu();
    fclose(pf);

    cpuF = NULL;
    }

/*---------------------------  End Of File  ------------------------------*/
