/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
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

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void initCyber(char *config);
static void initHelpers(void);
static void initNpuConnections(void);
static void initOperator(void);
static void initEquipment(void);
static void initDeadstart(void);
static bool initOpenSection(char *name);
static bool initGetOctal(char *entry, int defValue, long *value);
static bool initGetInteger(char *entry, int defValue, long *value);
static bool initGetString(char *entry, char *defString, char *str, int strLen);
static bool initParseIpAddress(char *ipStr, u32 *ip, u16 *port);
static void initToUpperCase(char *str);
char *initGetNextLine(void);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
bool          bigEndian;
extern u16    deadstartPanel[];
extern u8     deadstartCount;
ModelFeatures features;
ModelType     modelType;
char          persistDir[256];
char          displayName[32];

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static FILE *fcb;
static long sectionStart;
static char *startupFile = "cyber.ini";
static char deadstart[80];
static char equipment[80];
static char helpers[80];
static char npuConnections[80];
static char operator[80];
static long chCount;
static union
    {
    u32 number;
    u8  bytes[4];
    }
endianCheck;

static ModelFeatures features6400     = (IsSeries6x00);
static ModelFeatures featuresCyber73  = (IsSeries70 | HasInterlockReg | HasCMU);
static ModelFeatures featuresCyber173 =
    (IsSeries170 | HasStatusAndControlReg | HasCMU);
static ModelFeatures featuresCyber175 =
    (IsSeries170 | HasStatusAndControlReg | HasInstructionStack | HasIStackPrefetch | Has175Float);
static ModelFeatures featuresCyber840A =
    (IsSeries800 | HasNoCmWrap | HasFullRTC | HasTwoPortMux | HasMaintenanceChannel | HasCMU | HasChannelFlag
     | HasErrorFlag | HasRelocationRegLong | HasMicrosecondClock | HasInstructionStack | HasIStackPrefetch);
static ModelFeatures featuresCyber865 =
    (IsSeries800 | HasNoCmWrap | HasFullRTC | HasTwoPortMux | HasStatusAndControlReg
     | HasRelocationRegShort | HasMicrosecondClock | HasInstructionStack | HasIStackPrefetch | Has175Float);

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
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void initStartup(char *config, char *configFile)
    {
    startupFile = configFile;

    /*
    **  Open startup file.
    */
    fcb = fopen(startupFile, "rb");
    if (fcb == NULL)
        {
        perror(startupFile);
        exit(1);
        }

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

    initCyber(config);
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
**
**  Returns:        Pointer to line buffer
**
**------------------------------------------------------------------------*/
char *initGetNextLine(void)
    {
    static char lineBuffer[MaxLine];
    char        *cp;
    bool        blank;

    /*
    **  Get next lineBuffer.
    */
    do
        {
        if ((fgets(lineBuffer, MaxLine, fcb) == NULL)
            || (lineBuffer[0] == '['))
            {
            /*
            **  End-of-file or end-of-section - return failure.
            */
            return (NULL);
            }

        /*
        **  Determine if this line consists only of whitespace or comment and
        **  replace all whitespace by proper space.
        */
        blank = TRUE;
        for (cp = lineBuffer; *cp != 0; cp++)
            {
            if (blank && (*cp == ';'))
                {
                break;
                }

            if (isspace(*cp))
                {
                *cp = (*cp == '\r' || *cp == '\n') ? '\0' : ' ';
                }
            else
                {
                blank = FALSE;
                }
            }
        } while (blank);

    /*
    **  Found a non-blank line - return to caller.
    */
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
    char model[40];
    char dummy[256];
    long memory;
    long ecsBanks;
    long esmBanks;
    long enableCejMej;
    long clockIncrement;
    long pps;
    long mask;
    long port;
    long conns;
    long setMHz;

    if (!initOpenSection(config))
        {
        fprintf(stderr, "(init   ) Required section [%s] not found in %s\n", config, startupFile);
        exit(1);
        }

    /*
    **  Check for obsolete keywords and abort if found.
    */
    if (initGetOctal("channels", 020, &chCount))
        {
        fprintf(stderr, "(init   ) ***WARNING*** Entry 'channels' obsolete in section [%s] in %s,\n", config, startupFile);
        fprintf(stderr, "                        channel count is determined from PP count.\n");
        exit(1);
        }

    if (initGetString("cmFile", "", dummy, sizeof(dummy)))
        {
        fprintf(stderr, "(init   ) ***WARNING*** Entry 'cmFile' obsolete in section [%s] in %s,\n", config, startupFile);
        fprintf(stderr, "                        please use 'persistDir' instead.\n");
        exit(1);
        }

    if (initGetString("ecsFile", "", dummy, sizeof(dummy)))
        {
        fprintf(stderr, "(init   ) ***WARNING*** Entry 'ecsFile' obsolete in section [%s] in %s,\n", config, startupFile);
        fprintf(stderr, "                        please use 'persistDir' instead.\n");
        exit(1);
        }

    if (initGetString("displayName", "CYBER Console", displayName, sizeof(displayName)))
        {
        fprintf(stderr, "(init   ) Consoles will be labeled '%s',\n", displayName);
        }

    /*
    **  Determine mainframe model and setup feature structure.
    */
    (void)initGetString("model", "6400", model, sizeof(model));

    if (stricmp(model, "6400") == 0)
        {
        modelType = Model6400;
        features  = features6400;
        }
    else if (stricmp(model, "CYBER73") == 0)
        {
        modelType = ModelCyber73;
        features  = featuresCyber73;
        }
    else if (stricmp(model, "CYBER173") == 0)
        {
        modelType = ModelCyber173;
        features  = featuresCyber173;
        }
    else if (stricmp(model, "CYBER175") == 0)
        {
        modelType = ModelCyber175;
        features  = featuresCyber175;
        }
    else if (stricmp(model, "CYBER840A") == 0)
        {
//        modelType = ModelCyber840A;
//        features = featuresCyber840A;
        fprintf(stderr, "(init   ) Model CYBER840A was experimental and is no longer supported in %s,\n", startupFile);
        exit(1);
        }
    else if (stricmp(model, "CYBER865") == 0)
        {
        modelType = ModelCyber865;
        features  = featuresCyber865;
        }
    else
        {
        fprintf(stderr, "(init   ) Entry 'model' specified unsupported mainframe %s in section [%s] in %s\n", model, config, startupFile);
        exit(1);
        }

    (void)initGetInteger("CEJ/MEJ", 1, &enableCejMej);
    if (enableCejMej == 0)
        {
        features |= HasNoCejMej;
        }

    /*
    **  Determine CM size and ECS banks.
    */
    (void)initGetOctal("memory", 01000000, &memory);
    if (memory < 040000)
        {
        fprintf(stderr, "(init   ) Entry 'memory' less than 40000B in section [%s] in %s\n", config, startupFile);
        exit(1);
        }

    if (modelType == ModelCyber865)
        {
        if ((memory != 01000000)
            && (memory != 02000000)
            && (memory != 03000000)
            && (memory != 04000000))
            {
            fprintf(stderr, "(init   ) Cyber 170-865 memory must be configured in 262K increments in section [%s] in %s\n", config, startupFile);
            exit(1);
            }
        }

    (void)initGetInteger("ecsbanks", 0, &ecsBanks);
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
        fprintf(stderr, "(init   ) Entry 'ecsbanks' invalid in section [%s] in %s - correct values are 0, 1, 2, 4, 8 or 16\n", config, startupFile);
        exit(1);
        }

    (void)initGetInteger("esmbanks", 0, &esmBanks);
    switch (esmBanks)
        {
    case 0:
    case 1:
    case 2:
    case 4:
    case 8:
    case 16:
        break;

    default:
        fprintf(stderr, "(init   ) Entry 'esmbanks' invalid in section [%s] in %s - correct values are 0, 1, 2, 4, 8 or 16\n", config, startupFile);
        exit(1);
        }

    if ((ecsBanks != 0) && (esmBanks != 0))
        {
        fprintf(stderr, "(init   ) You can't have both 'ecsbanks' and 'esmbanks' in section [%s] in %s\n", config, startupFile);
        exit(1);
        }

    /*
    **  Determine where to persist data between emulator invocations
    **  and check if directory exists.
    */
    if (initGetString("persistDir", "", persistDir, sizeof(persistDir)))
        {
        struct stat s;
        if (stat(persistDir, &s) != 0)
            {
            fprintf(stderr, "(init   ) Entry 'persistDir' in section [%s] in %s\n", config, startupFile);
            fprintf(stderr, "          specifies non-existing directory '%s'.\n", persistDir);
            exit(1);
            }

        if ((s.st_mode & S_IFDIR) == 0)
            {
            fprintf(stderr, "(init   ) Entry 'persistDir' in section [%s] in %s\n", config, startupFile);
            fprintf(stderr, "          '%s' is not a directory.\n", persistDir);
            exit(1);
            }
        }
    else
        {
        fprintf(stderr, "(init   ) Entry 'persistDir' was not found in section [%s] in %s\n", config, startupFile);
        exit(1);
        }

    /*
    **  Initialise CPU.
    */
    cpuInit(model, memory, ecsBanks + esmBanks, ecsBanks != 0 ? ECS : ESM);

    /*
    **  Determine number of PPs and initialise PP subsystem.
    */
    (void)initGetOctal("pps", 012, &pps);
    if ((pps != 012) && (pps != 024))
        {
        fprintf(stderr, "(init   ) Entry 'pps' invalid in section [%s] in %s - supported values are 012 or 024 (octal)\n", config, startupFile);
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
        fprintf(stderr, "(init   ) Required entry 'deadstart' in section [%s] not found in %s\n", config, startupFile);
        exit(1);
        }

    /*
    **  Get cycle counter speed in MHz.
    */
    (void)initGetInteger("setMhz", 0, &setMHz);

    /*
    **  Get clock increment value and initialise clock.
    */
    (void)initGetInteger("clock", 0, &clockIncrement);

    rtcInit((u8)clockIncrement, setMHz);

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
        fprintf(stderr, "(init   ) Required entry 'equipment' in section [%s] not found in %s\n", config, startupFile);
        exit(1);
        }

    /*
    **  Get optional trace mask. If not specified, use compile time value.
    */
    if (initGetOctal("trace", 0, &mask))
        {
        traceMask = (u32)mask;
        }

    /*
    **  Get optional Telnet port number. If not specified, use default value.
    */
    initGetInteger("telnetport", 5000, &port);
    mux6676TelnetPort = (u16)port;

    /*
    **  Get optional max Telnet connections. If not specified, use default value.
    */
    initGetInteger("telnetconns", 4, &conns);
    mux6676TelnetConns = (u16)conns;

    /*
    **  Get optional Plato port number. If not specified, use default value.
    */
    initGetInteger("platoport", 5004, &port);
    platoPort = (u16)port;

    /*
    **  Get optional max Plato connections. If not specified, use default value.
    */
    initGetInteger("platoconns", 4, &conns);
    platoConns = (u16)conns;
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
    long         blockSize;
    int          claPort;
    char         *line;
    char         *token;
    long         tcpPort;
    int          numConns;
    u8           connType;
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
    long         networkValue;
    Pcb          *pcbp;
    long         pingInterval;
    TermRecoType recoType;
    char         strValue[256];

    npuNetPreset();

    if (strlen(npuConnections) == 0)
        {
        /*
        **  Default is the classic port 6610, 10 connections starting at CLA port 01 and raw TCP connection.
        */
        npuNetRegisterConnType(6610, 0x01, 10, ConnTypeRaw, &ncbp);

        return;
        }

    if (!initOpenSection(npuConnections))
        {
        fprintf(stderr, "(init   ) Section [%s] not found in %s\n", npuConnections, startupFile);
        exit(1);
        }

    /*
    **  Get host ID
    */
    (void)initGetString("hostID", "CYBER", npuNetHostID, HostIdSize);
    initToUpperCase(npuNetHostID);

    /*
    **  Get host IP address
    */
    (void)initGetString("hostIP", "127.0.0.1", strValue, sizeof(strValue));
    if (initParseIpAddress(strValue, &npuNetHostIP, NULL) == FALSE)
        {
        fprintf(stderr, "(init   ) Invalid 'hostIP' value %s in section [%s] of %s - correct values are IPv4 addresses\n",
                strValue, npuConnections, startupFile);
        exit(1);
        }

    /*
    **  Get optional coupler node number. If not specified, use default value of 1.
    */
    initGetInteger("couplerNode", 1, &networkValue);
    if ((networkValue < 1) || (networkValue > 255))
        {
        fprintf(stderr, "(init   ) Invalid 'couplerNode' value %ld in section [%s] of %s - correct values are 1..255\n",
                networkValue, npuConnections, startupFile);
        exit(1);
        }
    npuSvmCouplerNode = (u8)networkValue;

    /*
    **  Get optional NPU node number. If not specified, use default value of 2.
    */
    initGetInteger("npuNode", 2, &networkValue);
    if ((networkValue < 1) || (networkValue > 255))
        {
        fprintf(stderr, "(init   ) Invalid 'npuNode' value %ld in section [%s] of %s - correct values are 1..255\n",
                networkValue, npuConnections, startupFile);
        exit(1);
        }
    npuSvmNpuNode = (u8)networkValue;

    /*
    **  Get optional CDCNet node number. If not specified, use default value of 255.
    */
    initGetInteger("cdcnetNode", 255, &networkValue);
    if ((networkValue < 1) || (networkValue > 255))
        {
        fprintf(stderr, "(init   ) Invalid 'cdcnetNode' value %ld in section [%s] of %s - correct values are 1..255\n",
                networkValue, npuConnections, startupFile);
        exit(1);
        }
    cdcnetNode = (u8)networkValue;

    /*
    **  Get optional privileged TCP and UDP port offsets for CDCNet TCP/IP passive connections. If not specified,
    **  use default value of 6600.
    */
    initGetInteger("cdcnetPrivilegedTcpPortOffset", 6600, &networkValue);
    if ((networkValue < 0) || (networkValue > 64000))
        {
        fprintf(stderr, "(init   ) Invalid 'cdcnetPrivilegedTcpPortOffset' value %ld in section [%s] of %s - correct values are 0..64000\n",
                networkValue, npuConnections, startupFile);
        exit(1);
        }
    cdcnetPrivilegedTcpPortOffset = (u16)networkValue;

    initGetInteger("cdcnetPrivilegedUdpPortOffset", 6600, &networkValue);
    if ((networkValue < 0) || (networkValue > 64000))
        {
        fprintf(stderr, "(init   ) Invalid 'cdcnetPrivilegedUdpPortOffset' value %ld in section [%s] of %s - correct values are 0..64000\n",
                networkValue, npuConnections, startupFile);
        exit(1);
        }
    cdcnetPrivilegedUdpPortOffset = (u16)networkValue;

    /*
    **  Process all equipment entries.
    */
    if (!initOpenSection(npuConnections))
        {
        fprintf(stderr, "(init   ) Section [%s] not found in %s\n", npuConnections, startupFile);
        exit(1);
        }

    lineNo = -1;
    while ((line = initGetNextLine()) != NULL)
        {
        lineNo += 1;

        /*
        **  Parse initial keyword
        */
        token = strtok(line, ",=");
        if (token == NULL)
            {
            fprintf(stderr, "(init   ) Invalid syntax in section [%s] of %s, relative line %d\n",
                    npuConnections, startupFile, lineNo);
            exit(1);
            }

        if (strcmp(token, "terminals") == 0)
            {
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
            **     <local-ip>      IP address to use in NJE connections for local host (value of hostIP
            **                     config entry used by default)
            **     <local-port>    Local TCP port number on which to listen for connections (0 for no listening
            **                     port, e.g., rhasp)
            **     <ping-interval> Interval in seconds between pings during idle periods of NJE connection
            **     <remote-ip>     IP address of host to which to connect for Reverse HASP, LIP, and NJE
            **     <remote-name>   Name of node to which to connect for LIP and NJE
            **     <remote-port>   TCP port number to which Reverse HASP, NJE, or LIP will connect
            */
            token = strtok(NULL, ",");
            if ((token == NULL) || !isdigit(token[0]))
                {
                fprintf(stderr, "(init   ) Invalid TCP port number %s in section [%s] of %s, relative line %d\n",
                        token == NULL ? "NULL" : token, npuConnections, startupFile, lineNo);
                exit(1);
                }

            tcpPort = strtol(token, NULL, 10);
            if (tcpPort > 65535)
                {
                fprintf(stderr, "(init   ) Invalid TCP port number %ld in section [%s] of %s, relative line %d\n",
                        tcpPort, npuConnections, startupFile, lineNo);
                exit(1);
                }

            /*
            **  Parse starting CLA port number
            */
            token = strtok(NULL, ",");
            if ((token == NULL) || !isxdigit(token[0]))
                {
                fprintf(stderr, "(init   ) Invalid CLA port number %s in section [%s] of %s, relative line %d\n",
                        token == NULL ? "NULL" : token, npuConnections, startupFile, lineNo);
                exit(1);
                }

            networkValue = strtol(token, NULL, 16);
            if ((networkValue < 1) || (networkValue > 255))
                {
                fprintf(stderr, "(init   ) Invalid CLA port number %ld in section [%s] of %s, relative line %d\n",
                        networkValue, npuConnections, startupFile, lineNo);
                fprintf(stderr, "(init   ) CLA port numbers must be between 01 and FF, and expressed in hexadecimal\n");
                exit(1);
                }
            claPort = (u8)networkValue;

            /*
            **  Parse number of connections on this port.
            */
            token = strtok(NULL, ",");
            if ((token == NULL) || !isdigit(token[0]))
                {
                fprintf(stderr, "(init   ) Invalid number of connections %s in section [%s] of %s, relative line %d\n",
                        token == NULL ? "NULL" : token, npuConnections, startupFile, lineNo);
                exit(1);
                }
            networkValue = strtol(token, NULL, 10);
            if ((networkValue < 0) || (networkValue > 100))
                {
                fprintf(stderr, "(init   ) Invalid number of connections %ld in section [%s] of %s, relative line %d\n",
                        networkValue, npuConnections, startupFile, lineNo);
                fprintf(stderr, "(init   ) Connection count must be between 0 and 100\n");
                exit(1);
                }
            numConns = (int)networkValue;

            /*
            **  Parse NPU connection type.
            */
            token = strtok(NULL, ", ");
            if (token == NULL)
                {
                fprintf(stderr, "(init   ) Invalid NPU connection type %s in section [%s] of %s, relative line %d\n",
                        token == NULL ? "NULL" : token, npuConnections, startupFile, lineNo);
                exit(1);
                }
            destHostAddr = NULL;
            blockSize    = DefaultBlockSize;

            if (strcmp(token, "raw") == 0)
                {
                connType = ConnTypeRaw;
                }
            else if (strcmp(token, "pterm") == 0)
                {
                connType = ConnTypePterm;
                }
            else if (strcmp(token, "rs232") == 0)
                {
                connType = ConnTypeRs232;
                }
            else if (strcmp(token, "telnet") == 0)
                {
                connType = ConnTypeTelnet;
                }
            else if (strcmp(token, "hasp") == 0)
                {
                /*
                **  terminals=<local-port>,<cla-port>,<connections>,hasp[,<block-size>]
                */
                connType  = ConnTypeHasp;
                blockSize = DefaultHaspBlockSize;
                token     = strtok(NULL, " ");
                if (token != NULL)
                    {
                    if ((*token == 'B') || (*token == 'b'))
                        {
                        blockSize = strtol(token + 1, NULL, 10);
                        if ((blockSize < MinBlockSize) || (blockSize > MaxBlockSize))
                            {
                            fprintf(stderr, "(init   ) Invalid block size %ld in section [%s] of %s, relative line %d\n",
                                    blockSize, npuConnections, startupFile, lineNo);
                            fprintf(stderr, "(init   ) Block size must be between %d and %d\n", MinBlockSize, MaxBlockSize);
                            exit(1);
                            }
                        }
                    else
                        {
                        fprintf(stderr, "(init   ) Invalid HASP block size specification '%s' in section [%s] of %s, relative line %d\n",
                                token, npuConnections, startupFile, lineNo);
                        exit(1);
                        }
                    }
                }
            else if (strcmp(token, "rhasp") == 0)
                {
                /*
                **   terminals=<local-port>,<cla-port>,<connections>,rhasp,<remote-ip>:<remote-port>[,<block-size>]
                */
                connType = ConnTypeRevHasp;
                token    = strtok(NULL, ", ");
                if (token == NULL)
                    {
                    fprintf(stderr, "(init   ) Missing remote host address in section [%s] of %s, relative line %d\n",
                            npuConnections, startupFile, lineNo);
                    exit(1);
                    }
                destHostAddr = token;
                if (initParseIpAddress(destHostAddr, &destHostIP, &destHostPort) == FALSE)
                    {
                    fprintf(stderr, "(init   ) Invalid Reverse HASP address '%s' in section [%s] of %s, relative line %d\n",
                            destHostAddr, npuConnections, startupFile, lineNo);
                    exit(1);
                    }
                if (destHostPort == 0)
                    {
                    fprintf(stderr, "(init   ) Missing port number on Reverse HASP address '%s' in section [%s] of %s, relative line %d\n",
                            destHostAddr, npuConnections, startupFile, lineNo);
                    exit(1);
                    }
                destHostName = destHostAddr;
                blockSize    = DefaultRevHaspBlockSize;
                token        = strtok(NULL, " ");
                if (token != NULL)
                    {
                    if ((*token == 'B') || (*token == 'b'))
                        {
                        blockSize = strtol(token + 1, NULL, 10);
                        if ((blockSize < MinBlockSize) || (blockSize > MaxBlockSize))
                            {
                            fprintf(stderr, "(init   ) Invalid block size %ld in section [%s] of %s, relative line %d\n",
                                    blockSize, npuConnections, startupFile, lineNo);
                            fprintf(stderr, "(init   ) Block size must be between %d and %d\n", MinBlockSize, MaxBlockSize);
                            exit(1);
                            }
                        }
                    else
                        {
                        fprintf(stderr, "(init   ) Invalid Reverse HASP block size specification '%s' in section [%s] of %s, relative line %d\n",
                                token, npuConnections, startupFile, lineNo);
                        exit(1);
                        }
                    }
                }
            else if (strcmp(token, "nje") == 0)
                {
                /*
                **   terminals=<local-port>,<cla-port>,1,nje,<remote-ip>:<remote-port>,<remote-name> ...
                **       [,<local-ip>][,<block-size>]
                */
                if (numConns != 1)
                    {
                    fprintf(stderr, "(init   ) Invalid port count on NJE definition (must be 1) in section [%s] of %s, relative line %d\n",
                            npuConnections, startupFile, lineNo);
                    exit(1);
                    }
                connType = ConnTypeNje;
                token    = strtok(NULL, ", ");
                if (token == NULL)
                    {
                    fprintf(stderr, "(init   ) Missing remote NJE node address in section [%s] of %s, relative line %d\n",
                            npuConnections, startupFile, lineNo);
                    exit(1);
                    }
                destHostAddr = token;
                if (initParseIpAddress(destHostAddr, &destHostIP, &destHostPort) == FALSE)
                    {
                    fprintf(stderr, "(init   ) Invalid remote NJE node address %s in section [%s] of %s, relative line %d\n",
                            destHostAddr, npuConnections, startupFile, lineNo);
                    exit(1);
                    }
                if (destHostPort == 0)
                    {
                    destHostPort = 175;                    // default NJE/TCP port number
                    }
                token = strtok(NULL, ", ");
                if (token == NULL)
                    {
                    fprintf(stderr, "(init   ) Missing remote NJE node name in section [%s] of %s, relative line %d\n",
                            npuConnections, startupFile, lineNo);
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
                        blockSize = strtol(token + 1, NULL, 10);
                        if (blockSize < MinNjeBlockSize)
                            {
                            fprintf(stderr, "(init   ) Invalid block size %ld in section [%s] of %s, relative line %d\n",
                                    blockSize, npuConnections, startupFile, lineNo);
                            fprintf(stderr, "(init   ) Block size must be at least %d\n", MinNjeBlockSize);
                            exit(1);
                            }
                        }
                    else if ((*token == 'P') || (*token == 'p'))
                        {
                        pingInterval = strtol(token + 1, NULL, 10);
                        if (pingInterval < 0)
                            {
                            fprintf(stderr, "(init   ) Invalid ping interval %ld in section [%s] of %s, relative line %d\n",
                                    pingInterval, npuConnections, startupFile, lineNo);
                            exit(1);
                            }
                        }
                    else if (initParseIpAddress(token, &localHostIP, NULL) == FALSE)
                        {
                        fprintf(stderr, "(init   ) Invalid local NJE node address %s in section [%s] of %s, relative line %d\n",
                                token, npuConnections, startupFile, lineNo);
                        exit(1);
                        }
                    token = strtok(NULL, ", ");
                    }
                }
            else if (strcmp(token, "trunk") == 0)
                {
                /*
                **   terminals=<local-port>,<cla-port>,1,trunk,<remote-ip>:<remote-port>,<remote-name>,<coupler-node>
                */
                if (numConns != 1)
                    {
                    fprintf(stderr, "(init   ) Invalid port count on trunk definition (must be 1) in section [%s] of %s, relative line %d\n",
                            npuConnections, startupFile, lineNo);
                    exit(1);
                    }
                connType = ConnTypeTrunk;
                token    = strtok(NULL, ", ");
                if (token == NULL)
                    {
                    fprintf(stderr, "(init   ) Missing remote host address on trunk definition in section [%s] of %s, relative line %d\n",
                            npuConnections, startupFile, lineNo);
                    exit(1);
                    }
                destHostAddr = token;
                if (initParseIpAddress(destHostAddr, &destHostIP, &destHostPort) == FALSE)
                    {
                    fprintf(stderr, "(init   ) Invalid remote host IP address %s on trunk definition in section [%s] of %s, relative line %d\n",
                            destHostAddr, npuConnections, startupFile, lineNo);
                    exit(1);
                    }
                token = strtok(NULL, ", ");
                if (token == NULL)
                    {
                    fprintf(stderr, "(init   ) Missing remote node name on trunk definition in section [%s] of %s, relative line %d\n",
                            npuConnections, startupFile, lineNo);
                    exit(1);
                    }
                destHostName = token;
                initToUpperCase(destHostName);
                token = strtok(NULL, " ");
                if (token == NULL)
                    {
                    fprintf(stderr, "(init   ) Missing coupler node number on trunk definition in section [%s] of %s, relative line %d\n",
                            npuConnections, startupFile, lineNo);
                    exit(1);
                    }
                networkValue = strtol(token, NULL, 10);
                if ((networkValue < 1) || (networkValue > 255))
                    {
                    fprintf(stderr, "(init   ) Invalid coupler node number %ld on trunk definition in section [%s] of %s, relative line %d\n",
                            networkValue, npuConnections, startupFile, lineNo);
                    exit(1);
                    }
                destNode = (u8)networkValue;
                }
            else
                {
                fprintf(stderr, "(init   ) Invalid NPU connection type %s in section [%s] of %s, relative line %d\n",
                        token == NULL ? "NULL" : token, npuConnections, startupFile, lineNo);
                fprintf(stderr, "(init   ) NPU connection types must be one of: hasp, nje, pterm, raw, rhasp, rs232, telnet\n");
                exit(1);
                }

            /*
            **  Setup NPU connection type.
            */
            rc = npuNetRegisterConnType(tcpPort, claPort, numConns, connType, &ncbp);
            if (rc == NpuNetRegOk)
                {
                switch (connType)
                    {
                case ConnTypeRaw:
                case ConnTypePterm:
                case ConnTypeRs232:
                case ConnTypeTelnet:
                    token = strtok(NULL, " ");
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
                            fprintf(stderr, "(init   ) Unrecognized keyword \"%s\" on terminal definition in section [%s] of %s, relative line %d\n",
                                    token, npuConnections, startupFile, lineNo);
                            exit(1);
                            }
                        while (numConns-- > 0)
                            {
                            pcbp = npuNetFindPcb(claPort++);
                            pcbp->controls.async.recoType = recoType;
                            }
                        }
                    break;

                case ConnTypeRevHasp:
                case ConnTypeNje:
                case ConnTypeTrunk:
                    len            = strlen(destHostName) + 1;
                    ncbp->hostName = (char *)malloc(len);
                    if (ncbp->hostName == NULL)
                        {
                        fprintf(stderr, "(init   ) Failed to register terminal (out of memory) in section [%s] of %s, relative line %d\n",
                                npuConnections, startupFile, lineNo);
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
                        pcbp->controls.nje.inputBuf     = (u8 *)malloc(pcbp->controls.nje.blockSize);
                        if (pcbp->controls.nje.inputBuf == NULL)
                            {
                            fputs("(init   ) Out of memory, failed to allocate input buffer for NJE\n", stderr);
                            exit(1);
                            }
                        pcbp->controls.nje.outputBuf = (u8 *)malloc(pcbp->controls.nje.blockSize);
                        if (pcbp->controls.nje.outputBuf == NULL)
                            {
                            fputs("(init   ) Out of memory, failed to allocate output buffer for NJE\n", stderr);
                            exit(1);
                            }
                        }
                    else if (connType == ConnTypeTrunk)
                        {
                        pcbp = npuNetFindPcb(claPort);
                        pcbp->controls.lip.remoteNode = destNode;
                        }
                    break;

                case ConnTypeHasp:
                    pcbp = npuNetFindPcb(claPort);
                    pcbp->controls.hasp.blockSize = blockSize;
                    break;
                    }
                }
            switch (rc)
                {
            case NpuNetRegOk:
                break;

            case NpuNetRegOvfl:
                fprintf(stderr, "(init   ) Too many terminal and trunk definitions (max of %d), in section [%s] of %s, relative line %d\n",
                        MaxTermDefs, npuConnections, startupFile, lineNo);
                exit(1);

            case NpuNetRegDupTcp:
                fprintf(stderr, "(init   ) Duplicate TCP port %ld for terminal definition in section [%s] of %s, relative line %d\n",
                        tcpPort, npuConnections, startupFile, lineNo);
                exit(1);

            case NpuNetRegDupCla:
                fprintf(stderr, "(init   ) Duplicate CLA port in terminal set starting with 0x%02x for connection type %s in section [%s] of %s, relative line %d\n",
                        claPort, token, npuConnections, startupFile, lineNo);
                exit(1);

            case NpuNetRegNoMem:
                fprintf(stderr, "(init   ) Failed to register terminals (out of memory) in section [%s] of %s, relative line %d\n",
                        npuConnections, startupFile, lineNo);
                exit(1);

            default:
                fprintf(stderr, "(init   ) Failed to register terminals (unexpected rc=%d) in section [%s] of %s, relative line %d\n",
                        rc, npuConnections, startupFile, lineNo);
                exit(1);
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
    char *deviceName;
    int  eqNo;
    int  unitNo;
    int  channelNo;
    u8   deviceIndex;
    int  lineNo;

    if (!initOpenSection(equipment))
        {
        fprintf(stderr, "(init   ) Required section [%s] not found in %s\n", equipment, startupFile);
        exit(1);
        }

    printf("(init   ) Loading Equipment Section [%s] from %s\n", equipment, startupFile);

    /*
    **  Process all equipment entries.
    */
    lineNo = -1;
    while ((line = initGetNextLine()) != NULL)
        {
        lineNo += 1;

        /*
        **  Parse device type.
        */
        token = strtok(line, ",");
        if ((token == NULL) || (strlen(token) < 2))
            {
            fprintf(stderr, "(init   ) Section [%s], relative line %d, invalid device type %s in %s\n",
                    equipment, lineNo, token == NULL ? "NULL" : token, startupFile);
            exit(1);
            }

        for (deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++)
            {
            if (strcmp(token, deviceDesc[deviceIndex].id) == 0)
                {
                break;
                }
            }

        if (deviceIndex == deviceCount)
            {
            fprintf(stderr, "(init   ) Section [%s], relative line %d, unknown device %s in %s\n",
                    equipment, lineNo, token == NULL ? "NULL" : token, startupFile);
            exit(1);
            }

        /*
        **  Parse equipment number.
        */
        token = strtok(NULL, ",");
        if ((token == NULL) || (strlen(token) != 1) || !isoctal(token[0]))
            {
            fprintf(stderr, "(init   ) Section [%s], relative line %d, invalid equipment no %s in %s\n",
                    equipment, lineNo, token == NULL ? "NULL" : token, startupFile);
            exit(1);
            }

        eqNo = strtol(token, NULL, 8);

        /*
        **  Parse unit number.
        */
        token = strtok(NULL, ",");
        if ((token == NULL) || !isoctal(token[0]))
            {
            fprintf(stderr, "(init   ) Section [%s], relative line %d, invalid unit count %s in %s\n",
                    equipment, lineNo, token == NULL ? "NULL" : token, startupFile);
            exit(1);
            }

        unitNo = strtol(token, NULL, 8);

        /*
        **  Parse channel number.
        */
        token = strtok(NULL, ", ");
        if ((token == NULL) || (strlen(token) != 2) || !isoctal(token[0]) || !isoctal(token[1]))
            {
            fprintf(stderr, "(init   ) Section [%s], relative line %d, invalid channel no %s in %s\n",
                    equipment, lineNo, token == NULL ? "NULL" : token, startupFile);
            exit(1);
            }

        channelNo = strtol(token, NULL, 8);

        if ((channelNo < 0) || (channelNo >= chCount))
            {
            fprintf(stderr, "(init   ) Section [%s], relative line %d, channel no %s not permitted in %s\n",
                    equipment, lineNo, token == NULL ? "NULL" : token, startupFile);
            exit(1);
            }

        /*
        **  Parse optional file name.
        */
        deviceName = strtok(NULL, " ");

        /*
        **  Initialise device.
        */
        deviceDesc[deviceIndex].init((u8)eqNo, (u8)unitNo, (u8)channelNo, deviceName);
        }
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
    char *line;
    char *token;
    u8   lineNo;


    if (!initOpenSection(deadstart))
        {
        fprintf(stderr, "(init   ) Required section [%s] not found in %s\n", deadstart, startupFile);
        exit(1);
        }

    printf("(init   ) Loading DeadStart Section [%s] from %s\n", deadstart, startupFile);

    /*
    **  Process all deadstart panel switches.
    */
    lineNo = 0;
    while ((line = initGetNextLine()) != NULL && lineNo < MaxDeadStart)
        {
        /*
        **  Parse switch settings.
        */
        token = strtok(line, " ;\n");
        if ((token == NULL) || (strlen(token) != 4)
            || !isoctal(token[0]) || !isoctal(token[1])
            || !isoctal(token[2]) || !isoctal(token[3]))
            {
            fprintf(stderr, "(init   ) Section [%s], relative line %d, invalid deadstart setting %s in %s\n",
                    deadstart, lineNo, token == NULL ? "NULL" : token, startupFile);
            exit(1);
            }

        deadstartPanel[lineNo++] = (u16)strtol(token, NULL, 8);

        /*
         * Print the value so we know what we captured
         */
        printf("          Row %02d (%s):[", lineNo - 1, token);

        for (int c = 11; c >= 0; c--)
            {
            if ((deadstartPanel[lineNo - 1] >> c) & 1)
                {
                printf("1");
                }
            else
                {
                printf("0");
                }

            if ((c > 0) && (c % 3 == 0))
                {
                printf(" ");
                }
            }

        printf("]\n");
        }

    deadstartCount = lineNo + 1;
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
        fprintf(stderr, "(init   ) Section [%s] not found in %s\n", helpers, startupFile);
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
        fprintf(stderr, "(init   ) Section [%s] not found in %s\n", operator, startupFile);
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
    char lineBuffer[MaxLine];
    char section[40];
    u8   sectionLength = (u8)strlen(name) + 2;

    /*
    **  Build section label.
    */
    strcpy(section, "[");
    strcat(section, name);
    strcat(section, "]");

    /*
    **  Reset to beginning.
    */
    fseek(fcb, 0, SEEK_SET);

    /*
    **  Try to find section header.
    */
    do
        {
        if (fgets(lineBuffer, MaxLine, fcb) == NULL)
            {
            /*
            **  End-of-file - return failure.
            */
            return (FALSE);
            }
        //  20200905 (SJZ): Case Insensitive Comparison
        } while (strncasecmp(lineBuffer, section, sectionLength) != 0);

    /*
    **  Remember start of section and return success.
    */
    sectionStart = ftell(fcb);

    return (TRUE);
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
    char *pos;

    /*
    **  Leave room for zero terminator.
    */
    strLen -= 1;

    /*
    **  Reset to begin of section.
    */
    fseek(fcb, sectionStart, SEEK_SET);

    /*
    **  Try to find entry.
    */
    do
        {
        if ((line = initGetNextLine()) == NULL)
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
        //  20200905 (SJZ): Case Insensitive Comparison
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
