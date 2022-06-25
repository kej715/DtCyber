@ECHO Off
::
::  Invoke packaging and positioning
::

Echo :: %DATE% %TIME%
Echo ::     Output Directory    :   %1
Echo ::     Platform Target     :   %2
Echo ::     Configuration       :   %3
Echo :: .%~2%~3.


if .%~2%~3. == .x86Release. Goto %~2%~3 
if .%~2%~3. == .x64Release. Goto %~2%~3 
if .%~2%~3. == .x86Debug.   Goto %~2%~3 
if .%~2%~3. == .x64Debug.   Goto %~2%~3 

Echo :: INVALID CONFIGURATION SPECIFIED

Goto NormalExit

:x86Release
Echo :: x86 Release version being copied to %cd%
copy %~1pp.exe .\wintools\pp.%~2.%~3.exe
Goto NormalExit

:x86Debug
Echo :: x86 Debug version being copied to %cd%
copy %~1pp.exe .\wintools\pp.%~2.%~3.exe
Goto NormalExit

:x64Release
Echo :: x64 Release version being copied to %cd%
copy %~1pp.exe .\wintools\pp.%~2.%~3.exe
Echo :: Release version - overwriting pp.exe
copy %~1pp.exe .\wintools\pp.exe
Goto NormalExit

:x64Debug
Echo :: x64 Debug version being copied to %cd%
copy %~1pp.exe .\wintools\pp.%~2.%~3.exe
Goto NormalExit

:NormalExit
