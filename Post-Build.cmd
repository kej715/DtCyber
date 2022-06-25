@ECHO Off
::
::  Invoke packaging and positioning
::

Echo :: %DATE% %TIME%
Echo ::     Output Directory    :   %1
Echo ::     Platform Target     :   %2
Echo ::     Configuration       :   %3
Echo :: .%~2%~3.


Echo :: Copying Dependencies into the Target Directory %1

if .%~2%~3. == .x86Release. Goto %~2%~3 
if .%~2%~3. == .x64Release. Goto %~2%~3 
if .%~2%~3. == .x86Debug.   Goto %~2%~3 
if .%~2%~3. == .x64Debug.   Goto %~2%~3 

Echo :: INVALID CONFIGURATION SPECIFIED

Goto NormalExit

:x86Release
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /Q "x86Release.iss"
Goto NormalExit

:x86Debug
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /Q "x86Debug.iss"
Goto NormalExit

:x64Release
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /Q "x64Release.iss"
Goto NormalExit

:x64Debug
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /Q "x64Debug.iss"
Goto NormalExit

:NormalExit
