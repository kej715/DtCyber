#ifndef DCC6681_H
#define DCC6681_H
/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2014, Tom Hunter
**
**  Name: dcc6681.h
**
**  Description:
**      This file defines public constants and macros
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
**  -----------------
**  DCC6681 Constants
**  -----------------
*/

/*
**  Function codes.
*/
#define Fc6681Select                02000
#define Fc6681DeSelect              02100
#define Fc6681ConnectMode2          01000
#define Fc6681FunctionMode2         01100
#define Fc6681DccStatusReq          01200
#define Fc6681DevStatusReq          01300
#define Fc6681MasterClear           01700
                                    
#define Fc6681FunctionMode1         00000
#define Fc6681Connect4Mode1         04000
#define Fc6681Connect5Mode1         05000
#define Fc6681Connect6Mode1         06000
#define Fc6681Connect7Mode1         07000
#define Fc6681ConnectEquipmentMask  07000
#define Fc6681ConnectUnitMask       00777
#define Fc6681ConnectFuncMask       00777
                                    
#define Fc6681InputToEor            01400
#define Fc6681Input                 01500
#define Fc6681Output                01600
#define Fc6681IoModeMask            07700
#define Fc6681IoIosMask             00070
#define Fc6681IoBcdMask             00001
                                    
/*                                  
**      Status reply                
*/                                  
#define StFc6681Ready               00000
#define StFc6681Reject              00001
#define StFc6681IntReject           00003

/*
**  -----------------------
**  DCC6681 Macro Functions
**  -----------------------
*/

/*
**  ------------------------
**  DCC6681 Type Definitions
**  ------------------------
*/

/*
**  ---------------------------
**  DCC6681 Function Prototypes
**  ---------------------------
*/

/*
**  dcc6681.c
*/
DevSlot *dcc6681Attach(u8 channelNo, u8 eqNo, u8 unitNo, u8 devType);
DevSlot *dcc6681FindDevice(u8 channelNo, u8 equipmentNo, u8 devType);
void dcc6681Interrupt(bool status);

/*
**  ------------------------
**  Global DCC6681 variables
**  ------------------------
*/

#endif /* DCC6681_H */
/*---------------------------  End Of File  ------------------------------*/

