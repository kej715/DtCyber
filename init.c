/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2025, Tom Hunter, Kevin Jordan, Steven Zoppi
**
**  Name: init.c
**
**  Description:
**      Perform reading and validation of startup file and use configured
**      values to startup emulation.
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
#include <ctype.h>
#include <sys/stat.h>

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "const.h"
#include "types.h"
#include "proto.h"
#include "npu.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define MaxLine    512

//  Console display parameters.

#ifdef WIN32
#define FontName               "Lucida Console"
#define DefaultFontLarge       24
#define DefaultFontMedium      12
#define DefaultFontSmall       8
#define DefaultBG              RGB(0, 0, 0)
#define DefaultFG              RGB(0, 255, 0)
#else
#define DefaultBG              "black"
#define DefaultFG              "green"
#define FontName               "lucidatypewriter"
#define DefaultFontLarge       24
#define DefaultFontMedium      14
#define DefaultFontSmall       10
#endif


#define DefaultHeightLarge     30
#define DefaultHeightMedium    20
#define DefaultHeightSmall     15

#define DefaultHeightPX        800
#define DefaultWidthPX         1100

#define DefaultScaleX          10
#define DefaultScaleY          12

#define DefaultTimerRate       100

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#define isoctal(c)    ((c) >= '0' && (c) <= '7')

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/
typedef struct initConnType
    {
    char *name;
    int  ordinal;
    } InitConnType;

typedef struct initLine
    {
    struct initLine *next;
    char            *sourceFile;
    int             lineNo;
    char            *text;
    } InitLine;

typedef struct initSection
    {
    struct initSection *next;
    char               *name;
    InitLine           *lines;
    InitLine           *curLine;
    } InitSection;

typedef struct initVal
    {
    char *valName;                          /* Value name */
    char *sectionName;                      /* Section Group Name */
    char *valStatus;                        /* Status String */
    } InitVal;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void initAddLine(InitSection *section, char *fileName, int lineNo, char *text);
static void initConsole(void);
static void initCyber(char *config);
static InitSection *initFindSection(char *name);
static void initHelpers(void);
static void initNpuConnections(void);
static void initOperator(void);
static void initEquipment(void);
static void initDeadstart(void);
static int initLookupConnType(char *connType);
static int initLookupDeviceType(char *deviceType);
static bool initOpenSection(char *name);
static int initParseEquipmentDefn(char *defn, char *file, char *section, int lineNo,
                                  u8 *eqNo, u8 *unitNo, u8 *channelNo, char **deviceParams);
static int initParseTerminalDefn(char *defn, char *file, char *section, int lineNo,
                                 u16 *tcpPort, u8 *claPort, u8 *claPortCount, char **remainder);
static bool initGetHex(char *entry, int defValue, long *value);
static bool initGetInteger(char *entry, int defValue, long *value);
static bool initGetOctal(char *entry, int defValue, long *value);
static bool initGetString(char *entry, char *defString, char *str, int strLen);
static bool initParseIpAddress(char *ipStr, u32 *ip, u16 *port);
static void initReadStartupFile(FILE *fcb, char *fileName);
static void initToUpperCase(char *str);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
bool          bigEndian;
extern u16    deadstartPanel[];
extern u8     deadstartCount;
char          displayName[32];
ModelFeatures features;
bool          isCyber180;
ModelType     modelType;
NpuSoftware   npuSw = SwUndefined;
char          persistDir[256];

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static long        chCount = 040; // will be adjusted if PP count specified as 012
static InitSection *curSection;
static char        deadstart[80];
static union
    {
    u32 number;
    u8  bytes[4];
    } endianCheck;
static char        console[80];
static char        equipment[80];
static char        helpers[80];
static char        npuConnections[80];
static char        operator[80];
static InitSection *sections    = NULL;
static char        *startupFile = "cyber.ini";

static ModelFeatures features6400     = (IsSeries6x00);
static ModelFeatures featuresCyber73  = (IsSeries70 | HasInterlockReg | HasCMU);
static ModelFeatures featuresCyber173 =
    (IsSeries170 | HasStatusAndControlReg | HasCMU);
static ModelFeatures featuresCyber175 =
    (IsSeries170 | HasStatusAndControlReg | HasInstructionStack | HasIStackPrefetch | Has175Float);
static ModelFeatures featuresCyber860 =
    (IsSeries800 | IsCyber180 | HasNoCmWrap | HasFullRTC | HasTwoPortMux | HasMaintenanceChannel | HasCMU
     | HasChannelFlag | HasErrorFlag | HasRelocationRegLong | HasMicrosecondClock);
static ModelFeatures featuresCyber865 =
    (IsSeries800 | HasNoCmWrap | HasFullRTC | HasTwoPortMux | HasStatusAndControlReg
     | HasRelocationRegShort | HasMicrosecondClock | HasInstructionStack | HasIStackPrefetch | Has175Float);
static ModelFeatures featuresCyber875 =
    (IsSeries800 | HasNoCmWrap | HasFullRTC | HasTwoPortMux | HasStatusAndControlReg
     | HasRelocationRegShort | HasMicrosecondClock | HasInstructionStack | HasIStackPrefetch | Has175Float
     | IsCyber875);

static InitConnType connTypes[] =
    {
    "telnet", ConnTypeTelnet,
    "raw",    ConnTypeRaw,
    "pterm",  ConnTypePterm,
    "hasp",   ConnTypeHasp,
    "rhasp",  ConnTypeRevHasp,
    "nje",    ConnTypeNje,
    "trunk",  ConnTypeTrunk,
    "rs232",  ConnTypeRs232,
    NULL, -1
    };

static char *connTypeNames[] = // indexed by ordinal
    {
    "raw",
    "pterm",
    "rs232",
    "telnet",
    "hasp",
    "rhasp",
    "nje",
    "trunk"
    };

static InitVal sectVals[] =
    {
    "CEJ/MEJ",                       "cyber",   "Valid",
    "channels",                      "cyber",   "Deprecated",
    "clock",                         "cyber",   "Valid",
    "cmFile",                        "cyber",   "Deprecated",
    "console",                       "cyber",   "Valid",
    "cpus",                          "cyber",   "Valid",
    "deadstart",                     "cyber",   "Valid",
    "displayName",                   "cyber",   "Valid",
    "ecsBanks",                      "cyber",   "Valid",
    "ecsFile",                       "cyber",   "Deprecated",
    "equipment",                     "cyber",   "Valid",
    "esmBanks",                      "cyber",   "Valid",
    "helpers",                       "cyber",   "Valid",
    "idle",                          "cyber",   "Valid",
    "idleCycles",                    "cyber",   "Valid",
    "idleTime",                      "cyber",   "Valid",
    "ipAddress",                     "cyber",   "Valid",
    "memory",                        "cyber",   "Valid",
    "model",                         "cyber",   "Valid",
    "networkInterface",              "cyber",   "Valid",
    "npuConnections",                "cyber",   "Valid",
    "operator",                      "cyber",   "Valid",
    "osType",                        "cyber",   "Valid",
    "persistDir",                    "cyber",   "Valid",
    "platoConns",                    "cyber",   "Deprecated",
    "platoPort",                     "cyber",   "Deprecated",
    "pps",                           "cyber",   "Valid",
    "setMhz",                        "cyber",   "Valid",
    "telnetConns",                   "cyber",   "Deprecated",
    "telnetPort",                    "cyber",   "Deprecated",
    "trace",                         "cyber",   "Valid",

    "cdcnetNode",                    "npu",     "Valid",
    "cdcnetPrivilegedTcpPortOffset", "npu",     "Valid",
    "cdcnetPrivilegedUdpPortOffset", "npu",     "Valid",
    "couplerNode",                   "npu",     "Valid",
    "hostID",                        "npu",     "Valid",
    "hostIP",                        "npu",     "Deprecated",
    "idleNetBufs",                   "npu",     "Valid",
    "npuNode",                       "npu",     "Valid",
    "terminals",                     "npu",     "Valid",

    "colorBG",                       "console", "Valid",
    "colorFG",                       "console", "Valid",
    "fontLarge",                     "console", "Valid",
    "fontLargeHeight",               "console", "Valid",
    "fontMedium",                    "console", "Valid",
    "fontMediumHeight",              "console", "Valid",
    "fontName",                      "console", "Valid",
    "fontSmall",                     "console", "Valid",
    "fontSmallHeight",               "console", "Valid",
    "heightPX",                      "console", "Valid",
    "scaleX",                        "console", "Valid",
    "scaleY",                        "console", "Valid",
    "timerRate",                     "console", "Valid",
    "widthPX",                       "console", "Valid",


    NULL,                            NULL,      NULL
    };

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Read and process startup file.
**
**  Parameters:     Name        Description.
**                  config      name of the section to run
**                  configFile  pathname of startup file
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void initStartup(char *config, char *configFile)
    {
    char        *cp;
    FILE        *fcb;
    int         len;
    static char overlayFile[MaxFSPath];

    /*
    **  Check for the existence of a startup overlay file. If one exits,
    **  read it first. This will ensure that its definitions take precedence
    **  over definitions in the explicit startup file.
    */
    cp = strrchr(configFile, '.');
    if (cp != NULL)
        {
        len = (int)(cp - configFile);
        }
    else
        {
        len = (int)strlen(configFile);
        }
    memcpy(overlayFile, configFile, len);
    strcpy(overlayFile + len, ".ovl");

    fcb = fopen(overlayFile, "rb");
    if (fcb != NULL)
        {
        printf("(init   ) Reading Configuration Overlay File '%s'\n\n", overlayFile);
        initReadStartupFile(fcb, overlayFile);
        fclose(fcb);
        }

    /*
    **  Open startup file.
    */
    printf("(init   ) Reading Configuration File '%s'\n\n", configFile);
    fcb = fopen(configFile, "rb");
    if (fcb == NULL)
        {
        perror(configFile);
        exit(1);
        }
    initReadStartupFile(fcb, configFile);
    fclose(fcb);

    /*
    **  Determine endianness of the host.
    */
    endianCheck.bytes[0] = 0;
    endianCheck.bytes[1] = 0;
    endianCheck.bytes[2] = 0;
    endianCheck.bytes[3] = 1;
    bigEndian            = endianCheck.number == 1;

    /*
    **  Read and process cyber.ini file.
    */
    printf("\n%s\n", DtCyberVersion);
    printf("%s\n", DtCyberCopyright);
    printf("%s\n\n", DtCyberLicense);
    printf("Starting initialisation\n\n");

#ifdef _DEBUG
    printf("[Version Build Date: %s %s ( DEBUG )]\n\n", __DATE__, __TIME__);
#else
    printf("[Version Build Date: %s %s (RELEASE)]\n\n", __DATE__, __TIME__);
#endif

    curSection = NULL;

    initCyber(config);
    initConsole();
    initDeadstart();
    initNpuConnections();
    initEquipment();
    initOperator();
    initHelpers();

    if ((features & HasMaintenanceChannel) != 0)
        {
        mchInit(0, 0, ChMaintenance, NULL);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert endian-ness of 32 bit value,
**
**  Parameters:     Name        Description.
**
**  Returns:        Converted value.
**
**------------------------------------------------------------------------*/
u32 initConvertEndian(u32 value)
    {
    u32 result;

    result  = (value & 0xff000000) >> 24;
    result |= (value & 0x00ff0000) >> 8;
    result |= (value & 0x0000ff00) << 8;
    result |= (value & 0x000000ff) << 24;

    return (result);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Return next non-blank line in section
**
**  Parameters:     Name        Description.
**                  lineNo      pointer to line number (in/out param)
**
**  Returns:        Pointer to line buffer
**
**------------------------------------------------------------------------*/
char *initGetNextLine(int *lineNo)
    {
    static char lineBuffer[MaxLine];

    if ((curSection == NULL) || (curSection->curLine == NULL))
        {
        return NULL;
        }
    strcpy(lineBuffer, curSection->curLine->text);
    startupFile         = curSection->curLine->sourceFile;
    *lineNo             = curSection->curLine->lineNo;
    curSection->curLine = curSection->curLine->next;

    return (lineBuffer);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Open the helpers section.
**
**  Parameters:     Name        Description.
**
**  Returns:        -1 if error
**                   0 if section not defined
**                   1 if section opened
**
**------------------------------------------------------------------------*/
int initOpenHelpersSection(void)
    {
    if (strlen(helpers) == 0)
        {
        return 0;
        }
    else if (initOpenSection(helpers))
        {
        return 1;
        }
    else
        {
        return -1;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Open the operator section.
**
**  Parameters:     Name        Description.
**
**  Returns:        -1 if error
**                   0 if section not defined
**                   1 if section opened
**
**------------------------------------------------------------------------*/
int initOpenOperatorSection(void)
    {
    if (strlen(operator) == 0)
        {
        return 0;
        }
    else if (initOpenSection(operator))
        {
        return 1;
        }
    else
        {
        return -1;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Open the console section.
**
**  Parameters:     Name        Description.
**
**  Returns:        -1 if error
**                   0 if section not defined
**                   1 if section opened
**
**------------------------------------------------------------------------*/
int initOpenConsoleSection(void)
    {
    if (strlen(console) == 0)
        {
        return 0;
        }
    else if (initOpenSection(console))
        {
        return 1;
        }
    else
        {
        return -1;
        }
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Read and process [cyber] startup file section.
**
**  Parameters:     Name        Description.
**                  config      name of the section to run
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void initCyber(char *config)
    {
    int  base;
    long clockIncrement;
    long conns;
    char *cp;
    long cpus;
    char dummy[256];
    long dummyInt;
    long ecsBanks;
    long enableCejMej;
    long esmBanks;
    bool isOk;
    long mask;
    long memory;
    char model[40];
    long port;
    long pps;
    int  rc;
    long setMHz;

    /*-------------------START OF PRECHECK-------------------*/

    /*
     *  Pre-Check all of the parameters of this section
     */
    if (!initOpenSection(config))
        {
        logDtError(LogErrorLocation, "Required section [%s] not found in %s\n", config, startupFile);
        exit(1);
        }

    printf("(init   ) Loading root section [%s] from %s\n", config, startupFile);

    int     lineNo = 0;
    char    *line;
    char    *token;
    InitVal *curVal;
    bool    goodToken = TRUE;
    int     numErrors = 0;

    while ((line = initGetNextLine(&lineNo)) != NULL)
        {
        token = strtok(line, "=");
        if (strlen(token) > 2)
            {
            goodToken = FALSE;
            for (curVal = sectVals; curVal->valName != NULL; curVal++)
                {
                if (strcasecmp(curVal->sectionName, "cyber") == 0)
                    {
                    if (strcasecmp(curVal->valName, token) == 0)
                        {
                        goodToken = TRUE;
                        logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: %-12s %s\n",
                                   startupFile, config, lineNo, token == NULL ? "NULL" : token, curVal->valStatus);
                        break;
                        }
                    }
                }
            if (!goodToken)
                {
                logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: invalid or deprecated configuration keyword '%s'\n",
                           startupFile, config, lineNo, token == NULL ? "NULL" : token);
                numErrors++;
                }
            }
        }

    if (numErrors > 0)
        {
        logDtError(LogErrorLocation, "Correct the %d error(s) in section '[%s]' of '%s' and restart.\n",
                   numErrors, config, startupFile);
        exit(1);
        }

    /*-------------------END OF PRECHECK-------------------*/

    if (!initOpenSection(config))
        {
        logDtError(LogErrorLocation, "Required section [%s] not found in '%s'\n", config, startupFile);
        exit(1);
        }
    lineNo = 0;

    /*
    **  Check for obsolete keywords and abort if found.
    */
    if (initGetOctal("channels", 020, &chCount))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: ***WARNING*** Entry 'channels' is obsolete\n", startupFile, config);
        logDtError(LogErrorLocation, "                        channel count is determined from PP count.\n");
        exit(1);
        }

    if (initGetString("cmFile", "", dummy, sizeof(dummy)))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: ***WARNING*** Entry 'cmFile' is obsolete\n", startupFile, config);
        logDtError(LogErrorLocation, "                        please use 'persistDir' instead.\n");
        exit(1);
        }

    if (initGetString("ecsFile", "", dummy, sizeof(dummy)))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: ***WARNING*** Entry 'ecsFile' is obsolete\n", startupFile, config);
        logDtError(LogErrorLocation, "                        please use 'persistDir' instead.\n");
        exit(1);
        }

    if (initGetString("displayName", "DtCyber Console", displayName, sizeof(displayName)))
        {
        logDtError(LogErrorLocation, "Consoles will be labeled '%s',\n", displayName);
        }

    /*
    **  Determine mainframe model and setup feature structure.
    */
    (void)initGetString("model", "6400", model, sizeof(model));

    if (stricmp(model, "CDC6400") == 0 || stricmp(model, "6400") == 0)
        {
        modelType = Model6400;
        features  = features6400;
        }
    else if (stricmp(model, "CYBER73") == 0 || stricmp(model, "73") == 0)
        {
        modelType = ModelCyber73;
        features  = featuresCyber73;
        }
    else if (stricmp(model, "CYBER173") == 0 || stricmp(model, "173") == 0)
        {
        modelType = ModelCyber173;
        features  = featuresCyber173;
        }
    else if (stricmp(model, "CYBER175") == 0 || stricmp(model, "175") == 0)
        {
        modelType = ModelCyber175;
        features  = featuresCyber175;
        }
    else if (stricmp(model, "CYBER860") == 0 || stricmp(model, "860") == 0)
        {
        modelType = ModelCyber860;
        features  = featuresCyber860;
        }
    else if (stricmp(model, "CYBER865") == 0 || stricmp(model, "865") == 0)
        {
        modelType = ModelCyber865;
        features  = featuresCyber865;
        }
    else if (stricmp(model, "CYBER875") == 0 || stricmp(model, "875") == 0)
        {
        modelType = ModelCyber865;
        features  = featuresCyber875;
        }
    else
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: 'model' specified unsupported mainframe type '%s'\n",
                   config, startupFile, model);
        exit(1);
        }
    isCyber180 = (features & IsCyber180) != 0;

    (void)initGetInteger("CEJ/MEJ", 1, &enableCejMej);
    if (enableCejMej == 0)
        {
        features |= HasNoCejMej;
        }

    /*
    **  Determine CM size.
    */
    if (initGetString("memory", "", dummy, sizeof(dummy)))
        {
        cp = dummy;
        base = 8;
        isOk = TRUE;
        while (isOk && *cp != '\0')
            {
            if (isdigit(*cp))
                {
                if (!isoctal(*cp))
                    {
                    base = 10;
                    }
                cp += 1;
                }
            else if (*(cp + 1) == '\0')
                {
                switch (*cp)
                    {
                case 'K':
                case 'k':
                case 'M':
                case 'm':
                    base = 10;
                    break;
                default:
                    isOk = FALSE;
                    break;
                    }
                cp += 1;
                }
            else
                {
                isOk = FALSE;
                }
            }
        if (isOk == FALSE)
            {
            logDtError(LogErrorLocation, "file '%s' section [%s]: Invalid 'memory' value: %s\n", startupFile, config, dummy);
            exit(1);
            }
        memory = 0;
        cp = dummy;
        while (*cp != '\0')
            {
            if (isdigit(*cp))
                {
                memory = (memory * base) + (*cp - '0');
                }
            else
                {
                switch (*cp)
                    {
                case 'K':
                case 'k':
                    memory *= 1024;
                    break;
                case 'M':
                case 'm':
                    memory *= OneMegabyte;
                    break;
                default:
                    break;
                    }
                }
            cp += 1;
            }
        }
    else
        {
        memory = 01000000;
        }

    if (memory < 040000)
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: Entry 'memory' less than 40000B\n", startupFile, config);
        exit(1);
        }

    if (modelType == ModelCyber865)
        {
        if ((memory != 01000000)
            && (memory != 02000000)
            && (memory != 03000000)
            && (memory != 04000000)
            && (memory != 010000000)
            && (memory != 014000000)
            && (memory != 020000000))
            {
            logDtError(LogErrorLocation, "file '%s' section [%s]: Cyber 865 memory must be configured in 262K increments, and Cyber 875 memory must be configured in 1024K increments\n",
                       startupFile, config);
            exit(1);
            }
        if ((memory > 04000000) && ((features & IsCyber875) == 0))
            {
            fputs("(init   ) Model coerced to CYBER875 due to memory size\n", stdout);
            strcpy(model, "CYBER875");
            features = featuresCyber875;
            }
        else if ((memory < 04000000) && ((features & IsCyber875) != 0))
            {
            fputs("(init   ) Model coerced to CYBER865 due to memory size\n", stdout);
            strcpy(model, "CYBER865");
            features = featuresCyber865;
            }
        }
    else if (isCyber180)
        {
        isOk = (memory % OneMegabyte) == 0;
        switch (memory / OneMegabyte)
            {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 10:
        case 12:
        case 14:
        case 16:
        case 32:
        case 64:
        case 128:
        case 256:
        case 512:
        case 1024:
        case 2048:
            // valid memory size, do nothing
            break;
        default:
            isOk = FALSE;
            break;
            }
        if (isOk == FALSE)
            {
            logDtError(LogErrorLocation, "file '%s' section [%s]: Invalid Cyber 180 memory size: %ld\n",
                       startupFile, config, memory);
            exit(1);
            }
        }
    else if (memory > 256 * 1024)
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: Invalid Cyber 170 memory size: %ld\n",
                   startupFile, config, memory);
        exit(1);
        }

    /*
    **  Determine ECS/ESM banks.
    */
    (void)initGetInteger("ecsbanks", 0, &ecsBanks);
    (void)initGetInteger("esmbanks", 0, &esmBanks);

    if (isCyber180 && ((ecsBanks != 0) || (esmBanks != 0)))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: 'ecsbanks' and 'esmbanks' are invalid for Cyber 180\n", startupFile, config);
        exit(1);
        }

    if ((ecsBanks != 0) && (esmBanks != 0))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: 'ecsbanks' and 'esmbanks' are mutually exclusive\n", startupFile, config);
        exit(1);
        }

    switch (ecsBanks)
        {
    case 0:
    case 1:
    case 2:
    case 4:
    case 8:
    case 16:
        break;

    default:
        logDtError(LogErrorLocation, "file '%s' section [%s]: Entry 'ecsbanks' invalid - correct values are 0, 1, 2, 4, 8 or 16\n", startupFile, config);
        exit(1);
        }

    switch (esmBanks)
        {
    case 0:
    case 1:
    case 2:
    case 4:
    case 8:
    case 16:
        break;

    case 32:
    case 64:
    case 128:
        if (modelType != ModelCyber865)
            {
            logDtError(LogErrorLocation, "WARNING - file '%s' section [%s]: Entry 'esmbanks' - only CPU models CYBER865 and CYBER875 can fully access more than 16 banks of ESM\n",
                       startupFile, config);
            }
        break;

    default:
        if (modelType == ModelCyber865)
            {
            logDtError(LogErrorLocation, "file '%s' section [%s]: Entry 'esmbanks' invalid - correct values are 0, 1, 2, 4, 8, 16, 32, 64 or 128\n",
                       startupFile, config);
            }
        else
            {
            logDtError(LogErrorLocation, "file '%s' section [%s]: Entry 'esmbanks' invalid - correct values are 0, 1, 2, 4, 8 or 16\n", startupFile, config);
            }
        exit(1);
        }

    /*
    **  Determine the number of CPUs to use.
    */
    initGetInteger("cpus", 1, &cpus);
    if ((cpus < 1) || (cpus > MaxCpus))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: Entry 'cpus' invalid - correct values are 1 or 2\n", startupFile, config);
        exit(1);
        }
    cpuCount = (int)cpus;

    /*
    **  Determine where to persist data between emulator invocations
    **  and check if directory exists.
    */
    if (initGetString("persistDir", "", persistDir, sizeof(persistDir)))
        {
        struct stat s;
        if (stat(persistDir, &s) != 0)
            {
            logDtError(LogErrorLocation, "file '%s' section [%s]: Entry 'persistDir' specifies non-existing directory '%s'\n",
                       startupFile, config, persistDir);
            exit(1);
            }

        if ((s.st_mode & S_IFDIR) == 0)
            {
            logDtError(LogErrorLocation, "file '%s' section [%s]: Entry 'persistDir' specifies '%s' which is not a directory\n",
                       startupFile, config, persistDir);
            exit(1);
            }
        }
    else
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: Entry 'persistDir' is missing\n", startupFile, config);
        exit(1);
        }

    /*
    **  Initialise CPU.
    */
    cpuInit(model, memory, ecsBanks + esmBanks, ecsBanks != 0 ? ECS : ESM);
    if (ecsBanks + esmBanks == 0)
        {
        fprintf(stdout, "(init   ) Successfully configured model %s with %d CPUs.\n", model, cpuCount);
        }
    else
        {
        fprintf(stdout, "(init   ) Successfully configured model %s with %d CPUs and %ld banks of %s.\n", model, cpuCount, ecsBanks + esmBanks, ecsBanks != 0 ? "ESM" : "ECS");
        }


    /*
    **  Determine number of PPs and initialise PP subsystem.
    */
    (void)initGetOctal("pps", 012, &pps);
    if ((pps != 012) && (pps != 024))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: Entry 'pps' invalid - supported values are 012 or 024\n", startupFile, config);
        exit(1);
        }

    ppInit((u8)pps);

    /*
    **  Calculate number of channels and initialise channel subsystem.
    */
    if (pps == 012)
        {
        chCount = 020;
        }
    else
        {
        chCount = 040;
        }

    channelInit((u8)chCount);

    /*
    **  Get active deadstart switch section name.
    */
    if (!initGetString("deadstart", "", deadstart, sizeof(deadstart)))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s] Required entry 'deadstart' is missing\n", startupFile, config);
        exit(1);
        }

    /*
    **  Get cycle counter speed in MHz.
    */
    (void)initGetInteger("setMhz", 0, &setMHz);
    fprintf(stdout, "(init   ) %ld MHz set.\n", setMHz);

    /*
    **  Get clock increment value and initialise clock.
    */
    (void)initGetInteger("clock", 0, &clockIncrement);

    rtcInit((u8)clockIncrement, setMHz);
    fprintf(stdout, "(init   ) %ld Clock increment set.\n", clockIncrement);

    /*
    **  Initialise optional Interlock Register on channel 15.
    */
    if ((features & HasInterlockReg) != 0)
        {
        if (pps == 012)
            {
            ilrInit(64);
            }
        else
            {
            ilrInit(128);
            }
        }

    /*
    **  Initialise optional Status/Control Register on channel 16.
    */
    if ((features & HasStatusAndControlReg) != 0)
        {
        scrInit(ChStatusAndControl);
        if (pps == 024)
            {
            scrInit(ChStatusAndControl + 020);
            }
        }

    /*
    **  Get optional console section name.
    */
    initGetString("console", "", console, sizeof(console));

    /*
    **  Get optional helpers section name.
    */
    initGetString("helpers", "", helpers, sizeof(helpers));

    /*
    **  Get optional NPU port definition section name.
    */
    initGetString("npuConnections", "", npuConnections, sizeof(npuConnections));

    /*
    **  Get optional operator section name.
    */
    initGetString("operator", "", operator, sizeof(operator));

    /*
    **  Get active equipment section name.
    */
    if (!initGetString("equipment", "", equipment, sizeof(equipment)))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: Required entry 'equipment' is missing\n", startupFile, config);
        exit(1);
        }

    /*
    **  Get optional trace mask. If not specified, use compile time value.
    */
    if (initGetOctal("trace", 0, &mask))
        {
        traceMask = (u32)mask;
        }
    fprintf(stdout, "(init   ) 0x%08x Tracing mask set.\n", traceMask);

    /*
    **  Get optional IP address of DtCyber. If not specified, use "0.0.0.0".
    */
    initGetString("ipaddress", "0.0.0.0", ipAddress, 16);
    if (initParseIpAddress((strcmp(ipAddress, "0.0.0.0") == 0)? "127.0.0.1" : ipAddress, &npuNetHostIP, NULL) == FALSE)
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: Invalid 'ipAddress' value %s - correct values are IPv4 addresses\n",
                   startupFile, config, ipAddress);
        exit(1);
        }
    fprintf(stdout, "(init   ) IP address is '%s'\n", ipAddress);

    /*
    **  Get optional network interface name of DtCyber.
    */
    networkInterface[0]    = '\0';
    networkInterfaceMgr[0] = '\0';
    initGetString("networkinterface", "", dummy, sizeof(dummy));
    if (strlen(dummy) > 0)
        {
        char cmd[128];

        cp = dummy;
        while (*cp != '\0' && *cp != ',')
            {
            cp += 1;
            }
        if (*cp == ',')
            {
            *cp++ = '\0';
            }
        else
            {
            cp = "./ifcmgr";
            }
        if (strlen(dummy) > 16)
            {
            logDtError(LogErrorLocation, "file '%s' section [%s]: Invalid 'networkInterface' value %s\n",
                       startupFile, config, dummy);
            exit(1);
            }
        strcpy(networkInterface, dummy);
        strcpy(networkInterfaceMgr, cp);
        fprintf(stdout, "(init   ) Network interface is '%s'\n", networkInterface);
        sprintf(cmd, "%s %s %s start", networkInterfaceMgr, networkInterface, ipAddress);
        rc = runHelper(cmd);
        if (rc == 0)
            {
            fprintf(stdout, "(helper ) Started: %s %s %s\n", networkInterfaceMgr, networkInterface, ipAddress);
            }
        else
            {
            logDtError(LogErrorLocation, "Failed to start \"%s\", rc=%d'\n", networkInterfaceMgr, rc);
            exit(1);
            }
        }

    /*
    **  Get optional Telnet port number. If not specified, use default value.
    */
    initGetInteger("telnetport", 5000, &port);
    mux6676TelnetPort = (u16)port;
    if (port != 5000)
        {
        fprintf(stdout, "(init   ) mux6676 Telnet port %d set. (*** Note: deprecated ***)\n", mux6676TelnetPort);
        }

    /*
    **  Get optional max Telnet connections. If not specified, use default value.
    */
    initGetInteger("telnetconns", 4, &conns);
    mux6676TelnetConns = (u16)conns;
    if (conns != 4)
        {
        fprintf(stdout, "(init   ) mux6676 Telnet connections (max) %d Set. (*** Note: deprecated ***)\n", mux6676TelnetConns);
        }

    /* Get Idle loop settings */
    idle = FALSE;
    initGetString("idle", "off", dummy, sizeof(dummy));
    if ((strcasecmp(dummy, "on") == 0)
        || (strcasecmp(dummy, "true") == 0)
        || (strcasecmp(dummy, "1") == 0))
        {
        idle = TRUE;
        }
    else if ((strcasecmp(dummy, "off") != 0)
             && (strcasecmp(dummy, "false") != 0)
             && (strcasecmp(dummy, "0") != 0))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: Invalid value for 'idle' - must be one of 'on' or 'off'\n", startupFile, config);
        exit(1);
        }
#if defined(_WIN32)
    /* Sleep() on Windows seems to work OK with these defaults YMMV */
    initGetInteger("idlecycles", 7000, &dummyInt);
    idleTrigger = (u32)dummyInt;
    initGetInteger("idletime", 1, &dummyInt);
    idleTime = (u32)dummyInt;
#else
    initGetInteger("idlecycles", 50, &dummyInt);
    idleTrigger = (u32)dummyInt;
    initGetInteger("idletime", 60, &dummyInt);
    idleTime = (u32)dummyInt;
#endif

    if (idle)
        {
        fprintf(stdout, "(init   ) Idle every %d cycles for %d microseconds.\n",
                idleTrigger, idleTime);
        }
    else
        {
        fputs("(init   ) Idle off.\n", stdout);
        }

    /*
    **  Get optional operating system type. If not specified, use "none".
    **  Set idle loop detector function based upon operating system type.
    */
    initGetString("ostype", "none", osType, 16);
    if (strcasecmp(osType, "none") == 0)
        {
        idleDetector = &idleDetectorNone;
        }
    else if (strcasecmp(osType, "nos") == 0)
        {
        idleDetector = &idleDetectorNOS;
        }
    else if (strcasecmp(osType, "nosbe") == 0)
        {
        idleDetector = &idleDetectorNOSBE;
        }
    else if (strcasecmp(osType, "kronos") == 0)
        {
        idleDetector = &idleDetectorNOS;
        }
    else if (strcasecmp(osType, "mace") == 0)
        {
        idleDetector = &idleDetectorMACE;
        }
    else if (strcasecmp(osType, "cos") == 0)
        {
        idleDetector = &idleDetectorCOS;
        }
    else
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: WARNING: Unrecognized operating system type: '%s'\n",
                   startupFile, config, osType);
        }

    fprintf(stdout, "(init   ) Operating system type is '%s'.\n", osType);

    /*
    **  Get optional Plato port number. If not specified, use default value.
    */
    initGetInteger("platoport", 5004, &port);
    platoPort = (u16)port;
    if (port != 5004)
        {
        fprintf(stdout, "(init   ) PLATO port = %d. (*** Note: deprecated ***)\n", platoPort);
        }

    /*
    **  Get optional max Plato connections. If not specified, use default value.
    */
    initGetInteger("platoconns", 4, &conns);
    platoConns = (u16)conns;
    if (conns != 4)
        {
        fprintf(stdout, "(init   ) PLATO connections = %d. (*** Note: deprecated ***)\n", platoConns);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read and process [console] startup file section.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void initConsole(void)
    {
    /* Set Defaults */
#ifdef WIN32
    colorBG = DefaultBG;
    colorFG = DefaultFG;
#else
    strcpy(colorBG, DefaultBG);
    strcpy(colorFG, DefaultFG);
#endif
    strcpy(fontName, FontName);

    fontHeightLarge  = DefaultHeightLarge;
    fontHeightMedium = DefaultHeightMedium;
    fontHeightSmall  = DefaultHeightSmall;

    fontLarge  = DefaultFontLarge;
    fontMedium = DefaultFontMedium;
    fontSmall  = DefaultFontSmall;

    heightPX = DefaultHeightPX;
    widthPX  = DefaultWidthPX;

    scaleX = DefaultScaleX;
    scaleY = DefaultScaleY;

    timerRate = DefaultTimerRate;


    /*-------------------START OF PRECHECK-------------------*/

    /*
     *  Pre-Check all of the parameters of this section
     */

    if (!initOpenConsoleSection())
        {
        logDtError(LogErrorLocation, "Optional 'console' section [%s] not found in %s\n", console, startupFile);

        return;
        }

    printf("(init   ) Loading console section [%s] from %s\n", console, startupFile);

    int     lineNo = 0;
    char    *line;
    char    *token;
    InitVal *curVal;
    bool    goodToken = TRUE;
    int     numErrors = 0;
    int     ratio     = 0;

    while ((line = initGetNextLine(&lineNo)) != NULL)
        {
        token = strtok(line, "=");
        if (strlen(token) > 2)
            {
            goodToken = FALSE;
            for (curVal = sectVals; curVal->valName != NULL; curVal++)
                {
                if (strcasecmp(curVal->sectionName, "console") == 0)
                    {
                    if (strcasecmp(curVal->valName, token) == 0)
                        {
                        goodToken = TRUE;
                        logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: %-12s %s\n",
                                   startupFile, console, lineNo, token == NULL ? "NULL" : token, curVal->valStatus);
                        break;
                        }
                    }
                }
            if (!goodToken)
                {
                logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: invalid or deprecated configuration keyword '%s'\n",
                           startupFile, console, lineNo, token == NULL ? "NULL" : token);
                numErrors++;
                }
            }
        }

    if (numErrors > 0)
        {
        logDtError(LogErrorLocation, "Correct the %d error(s) in section '[%s]' of '%s' and restart.\n",
                   numErrors, console, startupFile);
        exit(1);
        }

    /*-------------------END OF PRECHECK-------------------*/

    // Reopen the section
    initOpenConsoleSection();
    lineNo = 0;


    if (initGetString("fontName", FontName, fontName, MaxFontNameSize))
        {
        logDtError(LogErrorLocation, "Font Name '%s' will be loaded\n", fontName);
        }
#ifdef WIN32
    (void)initGetHex("colorBG", DefaultBG, &colorBG);
    (void)initGetHex("colorFG", DefaultFG, &colorFG);
    //  Convert to RGB
    if (colorBG != DefaultBG)
        {
        colorBG = RGB(colorBG >> 16 & 0xff, colorBG >> 8 & 0xff, colorBG & 0xff);
        }
    //  Convert to RGB
    if (colorFG != DefaultFG)
        {
        colorFG = RGB(colorFG >> 16 & 0xff, colorFG >> 8 & 0xff, colorFG & 0xff);
        }
    if (colorBG == colorFG)
        {
        colorBG = DefaultBG;
        colorFG = DefaultFG;
        }
    printf("(init   )         [colorBG]=%06lx\n", colorBG);
    printf("(init   )         [colorFG]=%06lx\n", colorFG);
#else
    (void)initGetString("colorBG", DefaultBG, colorBG, sizeof(colorBG));
    (void)initGetString("colorFG", DefaultFG, colorFG, sizeof(colorFG));
    if (strcasecmp(colorBG, colorFG)==0)
    {
        strcpy(colorBG, DefaultBG);
        strcpy(colorFG, DefaultFG);
    }
    printf("(init   )         [colorBG]=%s\n", colorBG);
    printf("(init   )         [colorFG]=%s\n", colorFG);
#endif


    (void)initGetInteger("fontSmall", DefaultFontSmall, &fontSmall);
    (void)initGetInteger("fontMedium", DefaultFontMedium, &fontMedium);
    (void)initGetInteger("fontLarge", DefaultFontLarge, &fontLarge);
    if (fontSmall < 8)
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: 'fontSmall' must be greater than or equal to 8.\n", startupFile, console);
        numErrors += 1;
        }

    if (fontSmall > fontMedium)
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: 'fontSmall' must be smaller than 'fontMedium'\n", startupFile, console);
        numErrors += 1;
        }
    if (fontMedium > fontLarge)
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: 'fontMedium' must be smaller than 'fontLarge'\n", startupFile, console);
        numErrors += 1;
        }
    if (fontLarge > 48)
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: 'fontLarge' must be less than or equal to 48.\n", startupFile, console);
        numErrors += 1;
        }
    printf("(init   )         [fontSmall]=%ld\n", fontSmall);
    printf("(init   )         [fontMedium]=%ld\n", fontMedium);
    printf("(init   )         [fontLarge]=%ld\n", fontLarge);



    (void)initGetInteger("fontSmallHeight", DefaultHeightSmall, &fontHeightSmall);
    (void)initGetInteger("fontMediumHeight", DefaultHeightMedium, &fontHeightMedium);
    (void)initGetInteger("fontLargeHeight", DefaultHeightLarge, &fontHeightLarge);
    if (fontHeightSmall < 8)
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: 'fontSmallHeight' must be greater than or equal to 8.\n", startupFile, console);
        numErrors += 1;
        }

    if (fontHeightSmall > fontHeightMedium)
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: 'fontSmallHeight' must be smaller than 'fontMediumHeight'\n", startupFile, console);
        numErrors += 1;
        }
    if (fontHeightMedium > fontHeightLarge)
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: 'fontHeightMedium' must be smaller than 'fontHeightLarge'\n", startupFile, console);
        numErrors += 1;
        }
    if (fontHeightLarge > 48)
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: 'fontHeightLarge' must be less than or equal to 48.\n", startupFile, console);
        numErrors += 1;
        }

    printf("(init   )         [fontSmallHeight]=%ld\n", fontHeightSmall);
    printf("(init   )         [fontMediumHeight]=%ld\n", fontHeightMedium);
    printf("(init   )         [fontLargeHeight]=%ld\n", fontHeightLarge);

    (void)initGetInteger("scaleX", DefaultScaleX, &scaleX);
    (void)initGetInteger("scaleY", DefaultScaleY, &scaleY);
    if ((scaleX < 6) || (scaleX > 20))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: 'scaleX' must be between or equal to 6 and 20.\n", startupFile, console);
        numErrors += 1;
        }
    if ((scaleY < 6) || (scaleY > 20))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: 'scaleY' must be between or equal to 6 and 20.\n", startupFile, console);
        numErrors += 1;
        }

    (void)initGetInteger("timerRate", DefaultTimerRate, &timerRate);
    if ((timerRate < 50) || (timerRate > 200))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: 'timerRate' must be between or equal to 50 and 200.\n", startupFile, console);
        numErrors += 1;
        }
    printf("(init   )         [scaleX]=%ld\n", scaleX);
    printf("(init   )         [scaleY]=%ld\n", scaleY);

    (void)initGetInteger("widthPX", DefaultWidthPX, &widthPX);
    (void)initGetInteger("heightPX", DefaultHeightPX, &heightPX);
    if ((widthPX < 800) || (widthPX > 2560))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: 'widthPX' must be between or equal to 800 and 2560.\n", startupFile, console);
        numErrors += 1;
        }
    if ((heightPX < 600) || (heightPX > 1920))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: 'heightPX' must be between or equal to 600 and 1920.\n", startupFile, console);
        numErrors += 1;
        }
    ratio = (int)((float)heightPX / (float)widthPX * 100.0);
    if ((ratio < 56) || (ratio > 100))
        {
        logDtError(LogErrorLocation,
                   "file '%s' section [%s]: the ratio of 'heightPX' to 'widthPX' must be between 56%% (16:9) and 100%% (1:1). \n",
                   startupFile, console);
        logDtError(LogErrorLocation,
                   "Ratio of heightPX (%d) to widthPX (%d) is %d%%.", heightPX, widthPX, ratio);
        numErrors += 1;
        }
    printf("(init   )         [heightPX]=%ld\n", heightPX);
    printf("(init   )         [widthPX]=%ld\n", widthPX);

    if (numErrors > 0)
        {
        logDtError(LogErrorLocation, "Correct the %d error(s) in section '[%s]' of '%s' and restart.\n",
                   numErrors, console, startupFile);
        exit(1);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read and process NPU definitions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void initNpuConnections(void)
    {
    int          blockSize;
    u8           claPort;
    char         *cp;
    char         *line;
    char         *token;
    u16          tcpPort;
    u8           numConns;
    int          connType;
    int          len;
    int          lineNo;
    Ncb          *ncbp;
    int          rc;
    char         *destHostAddr;
    u32          destHostIP;
    u16          destHostPort;
    char         *destHostName;
    u8           destNode;
    u32          localHostIP;
    Pcb          *pcbp;
    long         pingInterval;
    TermRecoType recoType;
    char         *remainder;
    long         val;

    npuNetPreset();

    if (strlen(npuConnections) == 0)
        {
        /*
        **  Default is the classic port 6610, 10 connections starting at CLA port 01 and raw TCP connection.
        */
        npuNetRegisterConnType(6610, 0x01, 10, ConnTypeRaw, &ncbp);

        return;
        }

    /*-------------------START OF PRECHECK-------------------*/

    /*
     *  Pre-Check all of the parameters of this section
     */
    if (!initOpenSection(npuConnections))
        {
        logDtError(LogErrorLocation, "Required section [%s] not found in '%s'\n", npuConnections, startupFile);
        exit(1);
        }

    printf("(init   ) Loading NPU section [%s] from %s\n", npuConnections, startupFile);

    InitVal *curVal;
    bool    goodToken = TRUE;
    int     numErrors = 0;

    lineNo = 0;
    while ((line = initGetNextLine(&lineNo)) != NULL)
        {
        token = strtok(line, "=");
        if (strlen(token) > 2)
            {
            goodToken = FALSE;
            for (curVal = sectVals; curVal->valName != NULL; curVal++)
                {
                if (strcasecmp(curVal->sectionName, "npu") == 0)
                    {
                    if (strcasecmp(curVal->valName, token) == 0)
                        {
                        goodToken = TRUE;
                        logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: %-12s %s\n",
                                   startupFile, npuConnections, lineNo, token == NULL ? "NULL" : token, curVal->valStatus);
                        break;
                        }
                    }
                }
            if (!goodToken)
                {
                logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: invalid or deprecated configuration keyword %-12s %s\n",
                           startupFile, npuConnections, lineNo, token == NULL ? "NULL" : token,
                           curVal->valStatus == NULL ? "Invalid" : curVal->valStatus);
                numErrors++;
                }
            }
        }

    if (numErrors > 0)
        {
        logDtError(LogErrorLocation, "Correct the %d error(s) in section '[%s]' of '%s' and restart.\n",
                   numErrors, npuConnections, startupFile);
        exit(1);
        }

    /*-------------------END OF PRECHECK-------------------*/


    if (!initOpenSection(npuConnections))
        {
        logDtError(LogErrorLocation, "Section [%s] not found in '%s'\n", npuConnections, startupFile);
        exit(1);
        }

    /*
    **  Get host ID
    */
    (void)initGetString("hostID", "CYBER", npuNetHostID, HostIdSize);
    initToUpperCase(npuNetHostID);
    logDtError(LogErrorLocation, "Network host ID is '%s'\n", npuNetHostID);

    /*
    **  Get optional coupler node number. If not specified, use default value of 1.
    */
    initGetInteger("couplerNode", 1, &val);
    if ((val < 0) || (val > 255))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: Invalid 'couplerNode' value %ld - correct values are 1..255\n",
                   startupFile, npuConnections, val);
        exit(1);
        }
    npuSvmCouplerNode = (u8)val;
    logDtError(LogErrorLocation, "Host coupler node value is %d\n", npuSvmCouplerNode);

    /*
    **  Get optional NPU node number. If not specified, use default value of 2.
    */
    initGetInteger("npuNode", 2, &val);
    if ((val < 1) || (val > 255))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: Invalid 'npuNode' value %ld - correct values are 1..255\n",
                   startupFile, npuConnections, val);
        exit(1);
        }
    npuSvmNpuNode = (u8)val;
    logDtError(LogErrorLocation, "NPU node value is %d\n", npuSvmNpuNode);

    /*
    **  Get optional CDCNet node number. If not specified, use default value of 255.
    */
    initGetInteger("cdcnetNode", 255, &val);
    if ((val < 1) || (val > 255))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: Invalid 'cdcnetNode' value %ld - correct values are 1..255\n",
                   startupFile, npuConnections, val);
        exit(1);
        }
    cdcnetNode = (u8)val;
    logDtError(LogErrorLocation, "CDCNet node value is %d\n", cdcnetNode);

    /*
    **  Get optional privileged TCP and UDP port offsets for CDCNet TCP/IP passive connections. If not specified,
    **  use default value of 6600.
    */
    initGetInteger("cdcnetPrivilegedTcpPortOffset", 6600, &val);
    if ((val < 0) || (val > 64000))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: Invalid 'cdcnetPrivilegedTcpPortOffset' value %ld - correct values are 0..64000\n",
                   startupFile, npuConnections, val);
        exit(1);
        }
    cdcnetPrivilegedTcpPortOffset = (u16)val;
    logDtError(LogErrorLocation, "TCP privileged port offset is %d\n", cdcnetPrivilegedTcpPortOffset);

    initGetInteger("cdcnetPrivilegedUdpPortOffset", 6600, &val);
    if ((val < 0) || (val > 64000))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s]: Invalid 'cdcnetPrivilegedUdpPortOffset' value %ld - correct values are 0..64000\n",
                   startupFile, npuConnections, val);
        exit(1);
        }
    cdcnetPrivilegedUdpPortOffset = (u16)val;
    logDtError(LogErrorLocation, "UDP privileged port offset is %d\n", cdcnetPrivilegedUdpPortOffset);

    /*
    **  Get optional threshold value of network buffer backlog indicating that the NPU/MDI
    **  is busy.
    */
    initGetInteger("idleNetBufs", 4, &val);
    idleNetBufs = (u32)val;
    logDtError(LogErrorLocation, "Idle network buffer threshold is %d\n", idleNetBufs);

    /*
    **  Process all equipment entries.
    */
    if (!initOpenSection(npuConnections))
        {
        logDtError(LogErrorLocation, "Section [%s] not found in '%s'\n", npuConnections, startupFile);
        exit(1);
        }

    lineNo = 0;
    while ((line = initGetNextLine(&lineNo)) != NULL)
        {
        /*
        **  Parse initial keyword
        */
        for (cp = line; *cp != '\0' && *cp != ',' && *cp != '='; cp++)
            {
            }
        *cp = '\0';

        if (strcasecmp(line, "terminals") == 0)
            {
            *cp = '=';

            /*
            ** Parse terminals definition.  Syntax is:
            **
            **   terminals=<local-port>,<cla-port>,<connections>,hasp[,B<block-size>]
            **   terminals=<local-port>,<cla-port>,<connections>,nje,<remote-ip>:<remote-port>,<remote-name> ...
            **       [,<local-ip>][,B<block-size>][,P<ping-interval>]
            **   terminals=<local-port>,<cla-port>,<connections>,pterm[,auto|xauto]
            **   terminals=<local-port>,<cla-port>,<connections>,raw[,auto|xauto]
            **   terminals=<local-port>,<cla-port>,<connections>,rhasp,<remote-ip>:<remote-port>[,B<block-size>]
            **   terminals=<local-port>,<cla-port>,<connections>,rs232[,auto|xauto]
            **   terminals=<local-port>,<cla-port>,<connections>,telnet[,auto|xauto]
            **   terminals=<local-port>,<cla-port>,<connections>,trunk,<remote-ip>:<remote-port>,<remote-name>,<coupler-node>
            **
            **     terminal types:
            **       hasp   A TCP connection supporting HASP protocol
            **       nje    A TCP connection supporting NJE protocol
            **       pterm  A TCP connection supporting PTERM protocol for CYBIS
            **       raw    A raw TCP connection (i.e., no Telnet protocol)
            **       rhasp  A TCP connection supporting Reverse HASP protocol
            **       rs232  A connection supporting an RS-232 terminal server
            **       telnet A TCP connection supporting full Telnet protocol
            **       trunk  A TCP connection supporting LIP protocol (a trunk between DtCyber systems)
            **
            **     auto            Optional keyword indicating that an Async terminal connection is
            **     xauto           auto-configured. The corresponding NDL definition should specify
            **                     AUTO=YES or XAUTO=YES
            **     <block-size>    Maximum block size to send on HASP, Reverse HASP, and NJE connections
            **     <cla-port>      Starting CLA port number on NPU, in hexadecimal, must match NDL definition
            **     <coupler-node>  Coupler node number of DtCyber host at other end of trunk
            **     <connections>   Maximum number of concurrent connections to accept for port
            **     <local-ip>      IP address to use in NJE connections for local host (value of ipAddress
            **                     config entry used by default)
            **     <local-port>    Local TCP port number on which to listen for connections (0 for no listening
            **                     port, e.g., rhasp)
            **     <ping-interval> Interval in seconds between pings during idle periods of NJE connection
            **     <remote-ip>     IP address of host to which to connect for Reverse HASP, LIP, and NJE
            **     <remote-name>   Name of node to which to connect for LIP and NJE
            **     <remote-port>   TCP port number to which Reverse HASP, NJE, or LIP will connect
            */
            connType = initParseTerminalDefn(line, startupFile, npuConnections, lineNo,
                                             &tcpPort, &claPort, &numConns, &remainder);
            logDtError(LogErrorLocation, "[%s] line %2d: %6s TCP port %5d CLA port 0x%02x port count %3d\n",
                       npuConnections, lineNo, connTypeNames[connType], tcpPort, claPort, numConns);
            rc = npuNetRegisterConnType(tcpPort, claPort, numConns, connType, &ncbp);
            switch (rc)
                {
            case NpuNetRegOk:
                // success, do nothing
                break;

            case NpuNetRegOvfl:
                logDtError(LogErrorLocation, "Too many terminal and trunk definitions (max of %d)\n", MaxTermDefs);
                exit(1);

            case NpuNetRegDupTcp:
                logDtError(LogErrorLocation, "Duplicate TCP port %d\n", tcpPort);
                exit(1);

            case NpuNetRegDupCla:
                logDtError(LogErrorLocation, "Duplicate CLA port %d\n", claPort);
                exit(1);

            case NpuNetRegNoMem:
                logDtError(LogErrorLocation, "Failed to register terminals, out of memory\n");
                exit(1);

            default:
                logDtError(LogErrorLocation, "Failed to register terminals, unexpected error %d\n", rc);
                exit(1);
                }

            destHostAddr = NULL;
            blockSize    = DefaultBlockSize;

            switch (connType)
                {
            case ConnTypeRaw:
            case ConnTypePterm:
            case ConnTypeRs232:
            case ConnTypeTelnet:
                token = strtok(remainder, " ");
                if (token != NULL)
                    {
                    if (strcasecmp(token, "auto") == 0)
                        {
                        recoType = TermRecoAuto;
                        }
                    else if (strcasecmp(token, "xauto") == 0)
                        {
                        recoType = TermRecoXauto;
                        }
                    else
                        {
                        logDtError(LogErrorLocation, "Unrecognized keyword '%s'\n", token);
                        exit(1);
                        }
                    while (numConns-- > 0)
                        {
                        pcbp = npuNetFindPcb(claPort++);
                        pcbp->controls.async.recoType = recoType;
                        }
                    }
                break;

            case ConnTypeHasp:
                /*
                **  terminals=<local-port>,<cla-port>,<connections>,hasp[,<block-size>]
                */
                blockSize = DefaultHaspBlockSize;
                token     = strtok(remainder, " ");
                if (token != NULL)
                    {
                    if ((*token == 'B') || (*token == 'b'))
                        {
                        val = strtol(token + 1, NULL, 10);
                        if ((val < MinBlockSize) || (val > MaxBlockSize))
                            {
                            logDtError(LogErrorLocation, "Invalid block size %ld - correct block sizes are %d .. %d\n",
                                       val, MinBlockSize, MaxBlockSize);
                            exit(1);
                            }
                        blockSize = (int)val;
                        }
                    else
                        {
                        logDtError(LogErrorLocation, "Invalid block size specification '%s'\n", token);
                        exit(1);
                        }
                    }
                pcbp = npuNetFindPcb(claPort);
                pcbp->controls.hasp.blockSize = blockSize;
                logDtError(LogErrorLocation, "  block size %4d", blockSize);
                break;

            case ConnTypeRevHasp:
                /*
                **   terminals=<local-port>,<cla-port>,<connections>,rhasp,<remote-ip>:<remote-port>[,<block-size>]
                */
                token = strtok(remainder, ", ");
                if (token == NULL)
                    {
                    logDtError(LogErrorLocation, "Missing remote host address\n");
                    exit(1);
                    }
                destHostAddr = token;
                if (initParseIpAddress(destHostAddr, &destHostIP, &destHostPort) == FALSE)
                    {
                    logDtError(LogErrorLocation, "Invalid Reverse HASP address '%s'\n", destHostAddr);
                    exit(1);
                    }
                if (destHostPort == 0)
                    {
                    logDtError(LogErrorLocation, "Missing port number on Reverse HASP address '%s'\n", destHostAddr);
                    exit(1);
                    }
                destHostName = destHostAddr;
                blockSize    = DefaultRevHaspBlockSize;
                token        = strtok(NULL, " ");
                if (token != NULL)
                    {
                    if ((*token == 'B') || (*token == 'b'))
                        {
                        val = strtol(token + 1, NULL, 10);
                        if ((val < MinBlockSize) || (val > MaxBlockSize))
                            {
                            logDtError(LogErrorLocation, "Invalid block size %ld - correct block sizes are %d .. %d\n",
                                       val, MinBlockSize, MaxBlockSize);
                            exit(1);
                            }
                        blockSize = (int)val;
                        }
                    else
                        {
                        logDtError(LogErrorLocation, "Invalid Reverse HASP block size specification '%s'\n", token);
                        exit(1);
                        }
                    }
                logDtError(LogErrorLocation, "  block size %4d, destination host %s", blockSize, destHostName);
                break;

            case ConnTypeNje:
                /*
                **   terminals=<local-port>,<cla-port>,1,nje,<remote-ip>:<remote-port>,<remote-name> ...
                **       [,<local-ip>][,<block-size>]
                */
                if (numConns != 1)
                    {
                    logDtError(LogErrorLocation, "Invalid port count on NJE definition (must be 1)\n");
                    exit(1);
                    }
                token = strtok(remainder, ", ");
                if (token == NULL)
                    {
                    logDtError(LogErrorLocation, "Missing remote NJE node address\n");
                    exit(1);
                    }
                destHostAddr = token;
                if (initParseIpAddress(destHostAddr, &destHostIP, &destHostPort) == FALSE)
                    {
                    logDtError(LogErrorLocation, "Invalid remote NJE node address %s\n", destHostAddr);
                    exit(1);
                    }
                token = strtok(NULL, ", ");
                if (token == NULL)
                    {
                    logDtError(LogErrorLocation, "Missing remote NJE node name\n");
                    exit(1);
                    }
                destHostName = token;
                initToUpperCase(destHostName);
                localHostIP  = npuNetHostIP;
                blockSize    = DefaultNjeBlockSize;
                pingInterval = DefaultNjePingInterval;
                token        = strtok(NULL, ", ");
                while (token != NULL)
                    {
                    if ((*token == 'B') || (*token == 'b'))
                        {
                        val = strtol(token + 1, NULL, 10);
                        if (val < MinNjeBlockSize)
                            {
                            logDtError(LogErrorLocation, "Invalid block size %ld - correct block size is at least %d\n",
                                       val, MinNjeBlockSize);
                            exit(1);
                            }
                        blockSize = (int)val;
                        }
                    else if ((*token == 'P') || (*token == 'p'))
                        {
                        pingInterval = strtol(token + 1, NULL, 10);
                        if (pingInterval < 0)
                            {
                            logDtError(LogErrorLocation, "Invalid ping interval %ld\n", pingInterval);
                            exit(1);
                            }
                        }
                    else if (initParseIpAddress(token, &localHostIP, NULL) == FALSE)
                        {
                        logDtError(LogErrorLocation, "Invalid local NJE node address %s\n", token);
                        exit(1);
                        }
                    token = strtok(NULL, ", ");
                    }
                logDtError(LogErrorLocation, "  block size %4d, destination host %s/%s, source address %d.%d.%d.%d, ping interval %ld",
                           blockSize, destHostName, destHostAddr, 
                           (localHostIP >> 24) & 0xff, (localHostIP >> 16) & 0xff, (localHostIP >> 8) & 0xff, localHostIP & 0xff,
                           pingInterval);
                break;

            case ConnTypeTrunk:
                /*
                **   terminals=<local-port>,<cla-port>,1,trunk,<remote-ip>:<remote-port>,<remote-name>,<coupler-node>
                */
                if (numConns != 1)
                    {
                    logDtError(LogErrorLocation, "Invalid port count - must be 1\n");
                    exit(1);
                    }
                token = strtok(remainder, ", ");
                if (token == NULL)
                    {
                    logDtError(LogErrorLocation, "Missing remote host address\n");
                    exit(1);
                    }
                destHostAddr = token;
                if (initParseIpAddress(destHostAddr, &destHostIP, &destHostPort) == FALSE)
                    {
                    logDtError(LogErrorLocation, "Invalid remote host IP address %s\n", destHostAddr);
                    exit(1);
                    }
                token = strtok(NULL, ", ");
                if (token == NULL)
                    {
                    logDtError(LogErrorLocation, "Missing remote node name\n");
                    exit(1);
                    }
                destHostName = token;
                initToUpperCase(destHostName);

                token = strtok(NULL, " ");
                if (token == NULL)
                    {
                    logDtError(LogErrorLocation, "Missing coupler node number\n");
                    exit(1);
                    }
                val = strtol(token, NULL, 10);

                if ((val < 1) || (val > 255))
                    {
                    logDtError(LogErrorLocation, "Invalid coupler node number %ld\n", val);
                    exit(1);
                    }
                destNode = (u8)val;
                logDtError(LogErrorLocation, "  coupler node %d, destination host %s/%s",
                           destNode, destHostName, destHostAddr);
                break;
                }

            switch (connType)
                {
            case ConnTypeRevHasp:
            case ConnTypeNje:
            case ConnTypeTrunk:
                len            = (int)(strlen(destHostName) + 1);
                ncbp->hostName = (char *)malloc(len);
                if (ncbp->hostName == NULL)
                    {
                    logDtError(LogErrorLocation, "Out of memory\n");
                    exit(1);
                    }
                memcpy(ncbp->hostName, destHostName, len);
                memset(&ncbp->hostAddr, 0, sizeof(ncbp->hostAddr));
                ncbp->hostAddr.sin_family      = AF_INET;
                ncbp->hostAddr.sin_addr.s_addr = htonl(destHostIP);
                ncbp->hostAddr.sin_port        = htons(destHostPort);
                if (connType == ConnTypeNje)
                    {
                    pcbp = npuNetFindPcb(claPort);
                    pcbp->controls.nje.blockSize    = (int)blockSize;
                    pcbp->controls.nje.pingInterval = (int)pingInterval;
                    pcbp->controls.nje.localIP      = localHostIP;
                    pcbp->controls.nje.remoteIP     = destHostIP;
                    pcbp->controls.nje.inputBufSize = pcbp->controls.nje.blockSize * 2;
                    pcbp->controls.nje.inputBuf     = (u8 *)malloc(pcbp->controls.nje.inputBufSize);
                    if (pcbp->controls.nje.inputBuf == NULL)
                        {
                        logDtError(LogErrorLocation, "Out of memory\n");
                        exit(1);
                        }
                    pcbp->controls.nje.outputBuf = (u8 *)malloc(pcbp->controls.nje.blockSize);
                    if (pcbp->controls.nje.outputBuf == NULL)
                        {
                        logDtError(LogErrorLocation, "Out of memory\n");
                        exit(1);
                        }
                    }
                else if (connType == ConnTypeTrunk)
                    {
                    pcbp = npuNetFindPcb(claPort);
                    pcbp->controls.lip.remoteNode = destNode;
                    }
                break;

            default:
                break;
                }
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read and process equipment definitions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void initEquipment(void)
    {
    char *line;
    char *token;
    char *deviceParams;
    u8   eqNo;
    u8   unitNo;
    u8   channelNo;
    int  deviceIndex;
    int  lineNo;

    if (!initOpenSection(equipment))
        {
        logDtError(LogErrorLocation, "Required section [%s] not found in '%s'\n", equipment, startupFile);
        exit(1);
        }

    printf("(init   ) Loading equipment section [%s] from '%s'\n", equipment, startupFile);

    /*-------------------START OF PRECHECK-------------------*/

    /*
     *  Pre-Check that all of the entries in this section name
     *  valid equipment types.
     */
    int numErrors = 0;

    lineNo = 0;

    while ((line = initGetNextLine(&lineNo)) != NULL)
        {
        token = strtok(line, ",");
        if (token != NULL)
            {
            if (initLookupDeviceType(token) >= 0)
                {
                logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: %-10s Valid\n",
                           startupFile, equipment, lineNo, token == NULL ? "NULL" : token);
                }
            else
                {
                logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: invalid device type '%s'\n",
                           startupFile, equipment, lineNo, token == NULL ? "NULL" : token);
                numErrors++;
                }
            }
        else
            {
            logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: invalid device definition '%s'\n",
                       startupFile, equipment, lineNo, token == NULL ? "NULL" : line);
            numErrors++;
            }
        }

    if (numErrors > 0)
        {
        logDtError(LogErrorLocation, "Correct the %d error(s) in section '[%s]' of '%s' and restart.\n",
                   numErrors, equipment, startupFile);
        exit(1);
        }

    /*-------------------END OF PRECHECK-------------------*/

    initOpenSection(equipment);

    /*
    **  Process all equipment entries.
    */
    lineNo = 0;
    while ((line = initGetNextLine(&lineNo)) != NULL)
        {
        /*
        **  Parse device type and lookup device index.
        */
        deviceIndex = initParseEquipmentDefn(line, startupFile, equipment, lineNo,
                                             &eqNo, &unitNo, &channelNo, &deviceParams);

        /*
        **  Initialise device.
        */
        deviceDesc[deviceIndex].init(eqNo, unitNo, channelNo, deviceParams);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Parse an equipment definition.
**
**  Parameters:     Name         Description.
**                  defn         text of the definition
**                  file         name of file containing definition
**                  section      name of section containing definition
**                  lineNo       line number of the definition within its
**                               source section
**                  eqNo         parsed equipment number (output parameter)
**                  unitNo       parsed unit number (output parameter)
**                  channelNo    parsed channel number (output parameter)
**                  deviceParams pointer to device parameter string
**
**  Returns:        Device index
**
**------------------------------------------------------------------------*/
static int initParseEquipmentDefn(char *defn, char *file, char *section, int lineNo,
                                  u8 *eqNo, u8 *unitNo, u8 *channelNo, char **deviceParams)
    {
    char *token;
    u8   deviceIndex;

    /*
    **  Parse device type and lookup device index.
    */
    token = strtok(defn, ",");
    if (token != NULL)
        {
        deviceIndex = initLookupDeviceType(token);
        if (deviceIndex < 0)
            {
            logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: invalid device type '%s'\n",
                       file, section, lineNo, token == NULL ? "NULL" : token);
            exit(1);
            }
        }
    else
        {
        logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: invalid device definition '%s'\n",
                   file, section, lineNo, token == NULL ? "NULL" : defn);
        exit(1);
        }

    /*
    **  Parse equipment number.
    */
    token = strtok(NULL, ",");
    if ((token == NULL) || (strlen(token) != 1) || !isoctal(token[0]))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s] line %d: invalid equipment number %s\n",
                   file, section, lineNo, token == NULL ? "NULL" : token);
        exit(1);
        }

    *eqNo = (u8)strtol(token, NULL, 8);

    /*
    **  Parse unit number.
    */
    token = strtok(NULL, ",");
    if ((token == NULL) || !isoctal(token[0]))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s] line %d: invalid unit number %s\n",
                   file, section, lineNo, token == NULL ? "NULL" : token);
        exit(1);
        }

    *unitNo = (u8)strtol(token, NULL, 8);

    /*
    **  Parse channel number.
    */
    token = strtok(NULL, ", ");
    if ((token == NULL) || (strlen(token) != 2) || !isoctal(token[0]) || !isoctal(token[1]))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s] line %d: invalid channel number %s\n",
                   file, section, lineNo, token == NULL ? "NULL" : token);
        exit(1);
        }

    *channelNo = (u8)strtol(token, NULL, 8);
    if ((*channelNo < 0) || (*channelNo >= chCount))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s] line %d: invalid channel number %s\n",
                   file, section, lineNo, token == NULL ? "NULL" : token);
        exit(1);
        }

    /*
    **  Parse optional parameters.
    */
    *deviceParams = strtok(NULL, " ");
    if ((*deviceParams != NULL) && (**deviceParams == ';'))
        {
        *deviceParams = NULL;
        }

    return deviceIndex;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Parse a terminal definition.
**
**  Parameters:     Name         Description.
**                  defn         text of the definition
**                  file         name of file containing definition
**                  section      name of section containing definition
**                  lineNo       line number of the definition within its
**                               source section
**
**                  tcpPort      TCP listening port number (output parameter)
**                  claPort      CLA port number (output parameter)
**                  claPortCount CLA port count (output parameter)
**                  remainder    pointer to remaining parameter string
**
**  Returns:        Connection type
**
**------------------------------------------------------------------------*/
static int initParseTerminalDefn(char *defn, char *file, char *section, int lineNo,
                                 u16 *tcpPort, u8 *claPort, u8 *claPortCount,
                                 char **remainder)
    {
    int  connType;
    char *token;
    long val;

    token = strtok(defn, ",=");
    if ((token == NULL) || (strcasecmp(token, "terminals") != 0))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: Invalid terminal definition '%s'\n",
                   file, section, lineNo, token == NULL ? "NULL" : token);
        exit(1);
        }

    token = strtok(NULL, ",");
    if ((token == NULL) || !isdigit(token[0]))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: Invalid TCP port number %s\n",
                   file, section, lineNo, token == NULL ? "NULL" : token);
        exit(1);
        }

    val = strtol(token, NULL, 10);
    if (val > 65535)
        {
        logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: Invalid TCP port number %ld\n",
                   file, section, lineNo, val);
        exit(1);
        }
    *tcpPort = (u16)val;

    /*
    **  Parse starting CLA port number
    */
    token = strtok(NULL, ",");
    if ((token == NULL) || !isxdigit(token[0]))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: Invalid CLA port number %s\n",
                   file, section, lineNo, token == NULL ? "NULL" : token);
        exit(1);
        }

    val = strtol(token, NULL, 16);
    if ((val < 1) || (val > 255))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: Invalid CLA port number %ld\n",
                   file, section, lineNo, val);
        logDtError(LogErrorLocation, "CLA port numbers must be between 0x01 and 0xFF, expressed in hexadecimal\n");
        exit(1);
        }
    *claPort = (u8)val;

    /*
    **  Parse number of connections on this port.
    */
    token = strtok(NULL, ",");
    if ((token == NULL) || !isdigit(token[0]))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: Invalid number of connections %s\n",
                   file, section, lineNo, token == NULL ? "NULL" : token);
        exit(1);
        }

    val = strtol(token, NULL, 10);
    if ((val < 1) || (val > 255))
        {
        logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: Invalid number of connections %ld\n",
                   file, section, lineNo, val);
        logDtError(LogErrorLocation, "Connection count must be between 1 and 255\n");
        exit(1);
        }
    *claPortCount = (u8)val;

    /*
    **  Parse NPU connection type.
    */
    token = strtok(NULL, ", ");
    if (token == NULL)
        {
        logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: Invalid NPU connection type %s\n",
                   file, section, lineNo, token == NULL ? "NULL" : token);
        exit(1);
        }
    connType = initLookupConnType(token);
    if (connType == -1)
        {
        logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: Invalid NPU connection type %s\n",
                   file, section, lineNo, token);
        logDtError(LogErrorLocation, "NPU connection types must be one of: hasp, nje, pterm, raw, rhasp, rs232, telnet\n");
        exit(1);
        }

    *remainder = strtok(NULL, " ");

    return connType;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read and process deadstart panel settings.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void initDeadstart(void)
    {
    char *dp;
    int  dspi;
    bool isOk;
    char *line;
    int  lineNo;
    char *token;
    long word;

    if (!initOpenSection(deadstart))
        {
        logDtError(LogErrorLocation, "Required section [%s] not found in %s\n", deadstart, startupFile);
        exit(1);
        }

    printf("(init   ) Loading deadstart section [%s] from %s\n", deadstart, startupFile);

    /*
    **  Process all deadstart panel switches.
    */
    dspi   = 0;
    lineNo = 0;
    while ((line = initGetNextLine(&lineNo)) != NULL && dspi < MaxDeadStart)
        {
        /*
        **  Parse switch settings.
        */
        token = strtok(line, " ;\n");
        isOk = token != NULL;
        for (dp = token; isOk && *dp != '\0'; dp++)
            {
            isOk = isoctal(*dp);
            }
        if (isOk)
            {
            word = strtol(token, NULL, 8);
            isOk = (word < 010000) || (isCyber180 && (word >= 0100000 && word <= 0107777));
            }
        if (isOk)
            {
            deadstartPanel[dspi++] = (u16)word;
            }
        else
            {
            logDtError(LogErrorLocation, "file '%s' section [%s] line %2d: invalid deadstart panel setting %s\n",
                       startupFile, deadstart, lineNo, token == NULL ? "NULL" : token);
            exit(1);
            }

        /*
         * Print the value so we know what we captured
         */
        printf("          Row %02d", dspi - 1);
        if (isCyber180)
            {
            printf(" (%06lo)", word);
            }
        else
            {
            printf(" (%04lo)", word);
            }
        fputs(":[", stdout);

        for (int c = isCyber180 ? 15 : 11; c >= 0; c--)
            {
            fputc('0' + ((deadstartPanel[dspi - 1] >> c) & 1), stdout);
            if ((c > 0) && (c % 3 == 0))
                {
                fputc(' ', stdout);
                }
            }

        fputs("]\n", stdout);
        }

    deadstartCount = dspi + 1;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read and process helper definitions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void initHelpers(void)
    {
    if (strlen(helpers) == 0)
        {
        return;
        }

    if (initOpenHelpersSection() == -1)
        {
        logDtError(LogErrorLocation, "Section [%s] not found in %s\n", helpers, startupFile);
        exit(1);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read and process operator definitions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void initOperator(void)
    {
    if (strlen(operator) == 0)
        {
        return;
        }

    if (initOpenOperatorSection() == -1)
        {
        logDtError(LogErrorLocation, "Section [%s] not found in %s\n", operator, startupFile);
        exit(1);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Locate section header and remember the start of data.
**
**  Parameters:     Name        Description.
**                  name        section name
**
**  Returns:        TRUE if section was found, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool initOpenSection(char *name)
    {
    curSection = initFindSection(name);
    if (curSection != NULL)
        {
        curSection->curLine = curSection->lines;

        return TRUE;
        }
    else
        {
        return FALSE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Locate octal entry within section and return value.
**
**  Parameters:     Name        Description.
**                  entry       entry name
**                  defValue    default value
**                  value       pointer to return value
**
**  Returns:        TRUE if entry was found, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool initGetOctal(char *entry, int defValue, long *value)
    {
    char buffer[40];

    if (!initGetString(entry, "", buffer, sizeof(buffer))
        || (buffer[0] < '0')
        || (buffer[0] > '7'))
        {
        /*
        **  Return default value.
        */
        *value = defValue;

        return (FALSE);
        }

    /*
    **  Convert octal string to value.
    */
    *value = strtol(buffer, NULL, 8);

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Locate hexadecimal entry within section and return value.
**
**  Parameters:     Name        Description.
**                  entry       entry name
**                  defValue    default value
**                  value       pointer to return value
**
**  Returns:        TRUE if entry was found, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool initGetHex(char *entry, int defValue, long *value)
    {
    char buffer[40];

    //  Get the next entry
    if (!initGetString(entry, "", buffer, sizeof(buffer)))
        {
        /*
        **  Return default value.
        */
        *value = defValue;

        return (FALSE);
        }
    //  It must be valid Hexadecimal
    if (buffer[strspn(buffer, "0123456789abcdefABCDEF")] == 0)
        {
        /*
        **  Convert octal string to value.
        */
        *value = strtol(buffer, NULL, 16);

        return (TRUE);
        }

    /*
    **  Return default value.
    */
    *value = defValue;

    return (FALSE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Locate integer entry within section and return value.
**
**  Parameters:     Name        Description.
**                  entry       entry name
**                  defValue    default value
**                  value       pointer to return value
**
**  Returns:        TRUE if entry was found, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool initGetInteger(char *entry, int defValue, long *value)
    {
    char buffer[40];

    if (!initGetString(entry, "", buffer, sizeof(buffer))
        || (buffer[0] < '0')
        || (buffer[0] > '9'))
        {
        /*
        **  Return default value.
        */
        *value = defValue;

        return (FALSE);
        }

    /*
    **  Convert integer string to value.
    */
    *value = strtol(buffer, NULL, 10);

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Locate string entry within section and return string
**
**  Parameters:     Name        Description.
**                  entry       entry name
**                  defString   default string
**                  str         pointer to string buffer (return value)
**                  strLen      length of string buffer
**
**  Returns:        TRUE if entry was found, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool initGetString(char *entry, char *defString, char *str, int strLen)
    {
    u8   entryLength = (u8)strlen(entry);
    char *line;
    int  lineNo;
    char *pos;

    if (curSection == NULL)
        {
        return FALSE;
        }

    /*
    **  Leave room for zero terminator.
    */
    strLen -= 1;

    /*
    **  Reset to begin of section.
    */
    curSection->curLine = curSection->lines;

    /*
    **  Try to find entry.
    */
    lineNo = 0;
    do
        {
        if ((line = initGetNextLine(&lineNo)) == NULL)
            {
            /*
            **  Copy return value.
            */
            strncpy(str, defString, strLen);

            /*
            **  End-of-file or end-of-section - return failure.
            */
            return (FALSE);
            }
        } while (strncasecmp(line, entry, entryLength) != 0);

    /*
    **  Cut off any trailing comments.
    */
    pos = strchr(line, ';');
    if (pos != NULL)
        {
        *pos = 0;
        }

    /*
    **  Cut off any trailing whitespace.
    */
    pos = line + strlen(line) - 1;
    while (pos > line && isspace(*pos))
        {
        *pos-- = 0;
        }

    /*
    **  Locate start of value.
    */
    pos = strchr(line, '=');
    if (pos == NULL)
        {
        if (defString != NULL)
            {
            strncpy(str, defString, strLen);
            }

        /*
        **  No value specified.
        */
        return (FALSE);
        }

    /*
    **  Return value and success.
    */
    strncpy(str, pos + 1, strLen);

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Parse an IP address
**
**  Parameters:     Name        Description.
**                  ipStr       string representation of IP address
**                  ip          pointer to IP address return value
**                  port        pointer to port number return value
**
**  Returns:        TRUE if address parsed successfully, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool initParseIpAddress(char *ipStr, u32 *ip, u16 *port)
    {
    int  count;
    u32  result;
    long val;

    count  = 0;
    result = 0;
    while (*ipStr)
        {
        if ((*ipStr >= '0') && (*ipStr <= '9'))
            {
            val = 0;
            while (*ipStr >= '0' && *ipStr <= '9')
                {
                val = (val * 10) + (*ipStr++ - '0');
                }
            if ((val >= 0) && (val < 256))
                {
                result = (result << 8) | (u8)val;
                count += 1;
                if ((*ipStr == '.') && (count < 4))
                    {
                    ipStr += 1;
                    }
                else if (((*ipStr == '\0') || (*ipStr == ':')) && (count == 4))
                    {
                    *ip = result;
                    if (port != NULL)
                        {
                        if (*ipStr == ':')
                            {
                            val = strtol(ipStr + 1, NULL, 10);
                            if ((val >= 0) && (val <= 65535))
                                {
                                *port = (u16)val;
                                }
                            else
                                {
                                return (FALSE);
                                }
                            }
                        else
                            {
                            *port = 0;
                            }
                        }

                    return (TRUE);
                    }
                else
                    {
                    return (FALSE);
                    }
                }
            else
                {
                return (FALSE);
                }
            }
        else
            {
            return (FALSE);
            }
        }

    return (FALSE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read a startup configuration file and build a section
**                  chain from it
**
**  Parameters:     Name        Description.
**                  fcb         pointer to open file control block
**                  fileName    name of the file
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void initReadStartupFile(FILE *fcb, char *fileName)
    {
    char        *cp;
    InitSection *curSection;
    char        *lastNb;
    static char lineBuffer[MaxLine];
    int         lineNo;
    char        *sp;

    curSection = NULL;
    lineNo     = 0;

    while (fgets(lineBuffer, sizeof(lineBuffer), fcb) != NULL)
        {
        lineNo += 1;
        cp      = lineBuffer;
        if (*cp == '[')
            {
            sp = ++cp;
            while (TRUE)
                {
                if (*cp == ']')
                    {
                    break;
                    }
                else if ((*cp == '\0') || isspace(*cp))
                    {
                    *cp = '\0';
                    logDtError(LogErrorLocation, "Invalid section identifier starting with [\"%s\" in %s\n", sp, fileName);
                    exit(1);
                    }
                else
                    {
                    cp += 1;
                    }
                }
            *cp        = '\0';
            curSection = initFindSection(sp);
            if (curSection == NULL)
                {
                curSection = (InitSection *)calloc(1, sizeof(InitSection));
                if (curSection != NULL)
                    {
                    curSection->next = sections;
                    sections         = curSection;
                    curSection->name = (char *)malloc(strlen(sp) + 1);
                    if (curSection->name != NULL)
                        {
                        strcpy(curSection->name, sp);
                        }
                    }
                if ((curSection == NULL) || (curSection->name == NULL))
                    {
                    fputs("(init   ) Failed to allocate section structure\n", stderr);
                    exit(1);
                    }
                }
            lineNo = 0;
            }
        else
            {
            while (isspace(*cp))
                {
                cp += 1;
                }
            if ((*cp == '\0') || (*cp == ';'))
                {
                continue;                            // empty line or comment
                }
            sp = lastNb = cp++;
            while (*cp != '\0')
                {
                if ((*cp == '\n') || (*cp == '\r'))
                    {
                    break;
                    }
                if (isspace(*cp))
                    {
                    *cp = ' ';
                    }
                else
                    {
                    lastNb = cp;
                    }
                cp += 1;
                }
            *(lastNb + 1) = '\0';
            initAddLine(curSection, fileName, lineNo, sp);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Add a line to the list of lines in a section
**
**  Parameters:     Name        Description.
**                  section     point to section structure
**                  fileName    name of file from which line was read
**                  lineNo      line number of the line within the section
**                  text        the text of the line
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void initAddLine(InitSection *section, char *fileName, int lineNo, char *text)
    {
    u8       chNo1, chNo2;
    char     c;
    u8       claPort1, claPort2;
    u8       claPortCount;
    int      connType;
    char     *cp;
    InitLine *cursor;
    int      deviceIndex;
    char     *params1, *params2;
    u8       eqNo1, eqNo2;
    InitLine *line;
    char     lineBuffer[MaxLine];
    u16      tcpPort;
    u8       unitNo1, unitNo2;

    for (cp = text; *cp != '\0' && *cp != ',' && *cp != '='; cp++)
        {
        }
    c   = *cp;
    *cp = '\0';
    if (strcasecmp(text, "terminals") == 0)
        {
        *cp = c;
        strcpy(lineBuffer, text);
        connType = initParseTerminalDefn(lineBuffer, fileName, section->name, lineNo,
                                         &tcpPort, &claPort1, &claPortCount, &params1);
        //
        //  If a terminal definition specifying the same CLA port already exists,
        //  ignore this definition and allow the previous one to prevail.
        //
        for (line = section->lines; line != NULL; line = line->next)
            {
            if ((strncasecmp(line->text, "terminals", 9) != 0)
                || (strcmp(fileName, line->sourceFile) == 0))
                {
                continue;
                }
            strcpy(lineBuffer, line->text);
            connType = initParseTerminalDefn(lineBuffer, line->sourceFile, section->name, line->lineNo,
                                             &tcpPort, &claPort2, &claPortCount, &params2);
            if (claPort1 == claPort2)
                {
                return;
                }
            }
        }
    else if (initLookupDeviceType(text) >= 0)
        {
        *cp = c;
        strcpy(lineBuffer, text);
        deviceIndex = initParseEquipmentDefn(lineBuffer, fileName, section->name, lineNo,
                                             &eqNo1, &unitNo1, &chNo1, &params1);
        //
        //  If an equipment definition specifying the same equipment, unit, and channel
        //  number already exists, ignore this definition and allow the previous one to prevail.
        //
        for (line = section->lines; line != NULL; line = line->next)
            {
            if (strcmp(fileName, line->sourceFile) == 0)
                {
                continue;
                }
            strcpy(lineBuffer, line->text);
            deviceIndex = initParseEquipmentDefn(lineBuffer, line->sourceFile, section->name, line->lineNo,
                                                 &eqNo2, &unitNo2, &chNo2, &params2);
            if ((chNo1 == chNo2) && (eqNo1 == eqNo2) && (unitNo1 == unitNo2))
                {
                return;
                }
            }
        }
    else
        {
        *cp = c;
        }

    line = (InitLine *)calloc(1, sizeof(InitLine));
    if (line != NULL)
        {
        line->sourceFile = fileName;
        line->lineNo     = lineNo;
        line->text       = (char *)malloc(strlen(text) + 1);
        if (line->text != NULL)
            {
            strcpy(line->text, text);
            }
        if ((line == NULL) || (line->text == NULL))
            {
            fputs("(init   ) Failed to allocate section structure\n", stderr);
            exit(1);
            }
        }
    if (section->lines == NULL)
        {
        section->lines = line;
        }
    else
        {
        cursor = section->lines;
        while (cursor->next != NULL)
            {
            cursor = cursor->next;
            }
        cursor->next = line;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Find a named section in the section list
**
**  Parameters:     Name        Description.
**                  name        the section name to find
**
**  Returns:        Pointer to section structure, or NULL if not found.
**
**------------------------------------------------------------------------*/
static InitSection *initFindSection(char *name)
    {
    InitSection *section;

    for (section = sections; section != NULL; section = section->next)
        {
        if (strcasecmp(section->name, name) == 0)
            {
            return section;
            }
        }

    return NULL;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Lookup the ordinal of a connection type identifier
**
**  Parameters:     Name        Description.
**                  connType    the connection type identifier
**
**  Returns:        oridinal of connection type, or -1 if type not recognized
**
**------------------------------------------------------------------------*/
static int initLookupConnType(char *connType)
    {
    int i;

    for (i = 0; connTypes[i].name != NULL; i++)
        {
        if (strcasecmp(connType, connTypes[i].name) == 0)
            {
            return connTypes[i].ordinal;
            }
        }

    return -1;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Lookup the device index of a device type
**
**  Parameters:     Name        Description.
**                  deviceType  the type to lookup
**
**  Returns:        index of device type, or -1 if device type not found
**
**------------------------------------------------------------------------*/
static int initLookupDeviceType(char *deviceType)
    {
    int deviceIndex;

    for (deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++)
        {
        if (strcasecmp(deviceType, deviceDesc[deviceIndex].id) == 0)
            {
            return deviceIndex;
            }
        }

    return -1;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Translate a string to upper case
**
**  Parameters:     Name        Description.
**                  str         the string to translate
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void initToUpperCase(char *str)
    {
    while (*str != '\0')
        {
        if ((*str >= 'a') && (*str <= 'z'))
            {
            *str -= 0x20;
            }
        str += 1;
        }
    }

/*---------------------------  End Of File  ------------------------------*/
