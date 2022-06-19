@echo Off

:: --------------------------------------------------------------------------
:: 
::    Copyright (c) 2022, Steven Zoppi
:: 
::    Name: style.cmd
:: 
::    Description:
::        Build software engineering tools used in maintaining the
::        Desktop Cyber source code repository.
:: 
::    This program is free software: you can redistribute it and/or modify
::    it under the terms of the GNU General Public License version 3 as
::    published by the Free Software Foundation.
::    
::    This program is distributed in the hope that it will be useful,
::    but WITHOUT ANY WARRANTY; without even the implied warranty of
::    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
::    GNU General Public License version 3 for more details.
::    
::    You should have received a copy of the GNU General Public License
::    version 3 along with this program in file "license-gpl-3.0.txt".
::    If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.
:: 
:: --------------------------------------------------------------------------

setlocal ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

::
::  'where' command exists on modern versions of windows and
::  is a good test of how current this environment is ...
::
if exist "C:\Windows\System32\where.exe" goto :Start
set _returncode=10
set _errormessage="where" not found. Are you running Windows 10 or above^?
goto :exit

:Start

::
::  Check required files
::

PATH %~dp0;%PATH%

set _Required=pp.exe uncrustify.exe _SendToAttic.vbs _GetDate.vbs diff.exe tar.exe

for %%h in (!_Required!) do (
    where /Q %%h
    if !ERRORLEVEL! NEQ 0 (
        goto :FixPath
    )
)

goto :PathOK

:FixPath
echo ::
echo :: ^[NOTE^] Defaulting to distributed utilities
echo ::        One or more of the following were not available on
echo ::        the path: ^[!_Required!^]
echo ::

PATH %~dp0wintools;%PATH%

:PathOK

::
::  Establish our storage directory
::
set _storagedir=%~dp0_Attic
if exist "%_storagedir%" (
    Call :IsDirectory "%_storagedir%"
    set _returncode=!ERRORLEVEL!
    if !_returncode! NEQ 0 (
        set _errormessage="!_storagedir!" found, but is a file.  Must be directory.
        goto :exit
    )
) else (
    mkdir "%_storagedir%"
)

set _returncode=-1
set _errormessage=No Parameters Provided
If [%1] EQU [] goto :DisplayHelp

::
::  Create the archive file
::

::
::  Obtain date and time independent of OS Locale, Language or date format.
::  It becomes the name of the tar.gz file to which we will save before-images.
::  The resulting tarfile is stored in the same directory as this script.
::
Setlocal
For /f %%A in ('cscript //nologo %~dp0_getdate.vbs') do set _dtm=%%A
Set _yyyy=%_dtm:~0,4%
Set _mm=%_dtm:~4,2%
Set _dd=%_dtm:~6,2%
Set _hh=%_dtm:~8,2%
Set _nn=%_dtm:~10,2%
Set _ss=%_dtm:~12,2%
Echo The current date/time is: %_yyyy%-%_mm%-%_dd%_%_hh%.%_nn%.%_ss%
Endlocal&Set _dtm=%_YYYY%-%_mm%-%_dd%_%_hh%-%_nn%-%_ss%

set _tarfile=%_storagedir%\!_dtm!.tar
echo :: Before, Diff and After images will be written to:
echo :: File ^[!_tarfile!^]

::
::  Reset the return code and continue processing.
::
set _returncode=0

if /I NOT "%~x1" == ".txt" goto :FileSpec

::
::  Process a file list if specified
::

if EXIST "%~1" goto :InputFile

Echo :: The filelist specified ^[%~1^] does not exist.
set _returncode=2  
set _errormessage=List of Files does not exist
goto :Exit

:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::  Process each file indicated by the filespec
::
:FileSpec

for %%h in ("%~1") do (
    if exist "%%h" (
        Call :ProcFile "%%~dpnxh" "" 
    ) else (
        Echo :: ^[Skipping^] "%%~dpnxh" ^(Does not exist^)
    )
)
goto :Exit


:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::  Process files from specified input file
::
:InputFile    

for /f "tokens=* eol=;" %%h in (%~1) do Call :ProcFile "%%h" "%%~dp1" 

goto :Exit

:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::  Called Subroutine
::      Parameter   Description
::      1           Name of file
::      2           either empty-string or the path prefix for the file
::
:ProcFile
if /I "%~x1" == ".c" goto :GoodType
if /I "%~x1" == ".h" goto :GoodType
Echo :: Skipping requested File "%~1" ^(Invalid Type "%~x1"^)
exit /b

:GoodType
if exist "%~dp2%~1" goto :ProcFileFound
Echo :: File Not Found: "%~dp2%~1"
exit /b
::
::  Main Processing of individual files
::
:ProcFileFound
echo :: ^[Processing^] "%~dp2%~1"

set _errormessage=Processing Successful

::  Put the original file into the current tarfile 
::  (preserving last modified date and time just in case we have to restore)
if not exist "!_tarfile!" (
    tar -c -f "!_tarfile!" "%~dp2%~1"   
) else (
    tar -r -f "!_tarfile!" "%~dp2%~1"   
)

::  Put the dated copy of the original into the _Attic directory for quick
::  (additional) retrieval.
cscript //NOLOGO "%~dp0_SendToAttic.vbs"  "%~dp2%~1"

::  Reformat the code and put the output into the "formatted.txt" file.
uncrustify -c "%~dp0style.cfg" -f "%~dp2%~1" | pp   > "%~dp2%~1.formatted.txt"
diff -E -Z "%~dp2%~1" "%~dp2%~1.formatted.txt"      > "%~dp2%~1.diff"

::  Write the newly formatted replacement to the archive (before copy/rename)
tar -r -f "!_tarfile!" "%~dp2%~1.formatted.txt"     > NUL 2>&1
::  Write the diff to the TAR Archive
tar -r -f "!_tarfile!" "%~dp2%~1.diff"              > NUL 2>&1
::  Delete Original File
erase "%~dp2%~1"
::  Replace the original with the newly formatted file
rename "%~dp2%~1.formatted.txt" "%~nx1"
::  Delete the DIFF file
erase "%~dp2%~1.diff"

exit /b
::  End of ProcFile Subroutine
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

:IsDirectory
SETLOCAL
Set _attr=%~a1
Set _isdir=%_attr:~0,1%
If /I "%_isdir%"=="d" (
    REM %1 is a directory
    Exit /b 0
)
REM %1 is NOT a directory
Exit /b 2

:DisplayHelp

Echo ::
Echo :: %~n0 - Syntax:
Echo ::
Echo ::     %~n0[%~x0] ^<filespec^>
Echo ::
Echo :: where
Echo ::
Echo ::     ^<filespec^> is the name of a single file or
Echo ::                a wildcard specification identifying
Echo ::                multiple files.
Echo ::
Echo ::     No parameters provides this information.
Echo ::
Echo ::     Only files of type ".c" or ".h" will be processed.
Echo ::     If the file type of the first parameter is ".txt",
Echo ::     the contents of the file must contain a list of files
Echo ::     which are to be processed.
Echo ::
Echo ::     The list of files specified will always be relative
Echo ::     to the path containing the list.
Echo ::
Echo ::     Example:
Echo ::
Echo ::         If the list file is "C:\foo\list.txt" then
Echo ::         all files referenced within that .txt file
Echo ::         will automatically be prefixed with "C:\foo\"
Echo ::
Echo ::     Requisites:
for %%h in (!_Required!) do (
    where /T %%h
)

:Exit
endlocal & echo :: Result :: %_returncode% %_errormessage% & exit /b %_returncode%