Option Explicit

'   --------------------------------------------------------------------------
'   
'      Copyright (c) 2022, Steven Zoppi
'   
'      Name: _SentToAttic.vbs
'   
'      Description:
'
'          This script copies the specified file into the storage 
'          directory called "_Attic" (subordinate to the file being
'          copied) with the name altered to have the file's last
'          modified date appended to the filename.  The file extension
'          is the same as the original file.
'
'          This is a build support file called by "style.cmd".
'
'      Usage:
'
'          _SendToAttic <filename1> ... <filenameN>
'
'      Where:
'
'          <filename1...N> are names of INDIVIDUAL FILES which are to
'          be copied to the "Attic" directory.
'
'      Example:
'
'      File "C:\foo\operator.c" dated 31-dec-2022 at 15:16:17 is copied to
'           "C:\foo\_Attic\operator_20221231-151617.c"
'
'      License:
'   
'      This program is free software: you can redistribute it and/or modify
'      it under the terms of the GNU General Public License version 3 as
'      published by the Free Software Foundation.
'      
'      This program is distributed in the hope that it will be useful,
'      but WITHOUT ANY WARRANTY; without even the implied warranty of
'      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
'      GNU General Public License version 3 for more details.
'      
'      You should have received a copy of the GNU General Public License
'      version 3 along with this program in file "license-gpl-3.0.txt".
'      If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.
'   
'   --------------------------------------------------------------------------

Dim oFSO            : Set oFSO = CreateObject("Scripting.FilesystemObject")
Dim vArg            '   The Argument that we're currently processing
Dim sScriptName     : sScriptName = oFSO.GetFile(WScript.ScriptFullName).Name
Dim sScriptDir      : sScriptDir = oFSO.GetFile(WScript.ScriptFullName).ParentFolder.Path & "\"
Dim bDebug          : bDebug = False
Dim bQuiet          : bQuiet = True
Dim WshShell        : Set WshShell = WScript.CreateObject("WScript.Shell")


For Each vArg In WScript.Arguments
    If oFSO.FileExists(vArg) Then
        Call procFile(vArg)
    ElseIf oFSO.FolderExists(vArg) Then
        '   For the future
        '       Call procFolder(vArg)
    End If
Next

Function ProcFile(vArg)

    Dim fldCurrent      ' as Scripting.Folder
    Dim fldAttic        ' as Scripting.Folder
    Dim filInput        ' as Scripting.File
    Dim strAttic        '   Name of the attic folder
    Dim strAtticItem    '   New Item name for the 'backup'
    Dim strFileExt      '   File Extension
    Dim strFileName     '   File name UP TO the extension
    Dim strDateTime     '   Date/Time Stamp of the "Attic" folder
    Dim IXDot           '   Position of Extension delimiter
    Dim Return          '   Process Result
    Dim sPPCmd          '   PP Command
    
    Set filInput = oFSO.GetFile(vArg)   'as Scripting.File
    
    strAttic = filInput.ParentFolder.Path & "\_Attic"
    If Not oFSO.FolderExists(strAttic) Then
        If bDebug Then
            WScript.StdOut.WriteLine "DEBUG: Ready to create folder " & strAttic
        Else
            Call oFSO.CreateFolder(strAttic)
        End If
    End If
    
    IXDot = InStrRev( filInput.Name,".")
    strFileExt = Mid( filInput.Name, IXDot)
    strFileName = Left( filInput.Name, IXDot - 1)
    
    strDateTime = "_" & _
        RJ(Datepart("yyyy", filInput.DateLastModified),4,"0") & _
        RJ(Datepart("m", filInput.DateLastModified),2,"0") & _
        RJ(Datepart("d", filInput.DateLastModified),2,"0") & _
        "-" & _
        RJ(Datepart("h", filInput.DateLastModified),2,"0") & _
        RJ(Datepart("n", filInput.DateLastModified),2,"0") & _
        RJ(Datepart("s", filInput.DateLastModified),2,"0")
        
    strAtticItem = strAttic & "\" & strFileName & strDateTime & strFileExt
    
    '
    '   If the file already exists, just skip it.
    '
    If oFSO.FileExists(strAtticItem) Then
        If Not bQuiet Then
            WScript.StdOut.WriteLine sScriptName & ": Already Exists: " & strAtticItem
        End If
    Else
        If bDebug Then
            WScript.Echo "DEBUG: Ready to copy file " & vbCrLf & " From: " & vArg & vbcrlf & "   To: " & strAtticItem
        Else
            Call oFSO.CopyFile( vArg, strAtticItem )
        End If
        '
        '   If we are debugging, the rest isn't useful
        '        
        If bDebug Then
            WScript.StdOut.WriteLine "DEBUG: Nondeterministic Result."
        Else
            '
            '   This is where the actual copies are tested and we return
            '
            If oFSO.FileExists(strAtticItem) Then
                WScript.StdOut.WriteLine sScriptName & ": Copy Succeeded " & vbCrLf & _
                    "From " & vArg & vbcrlf & _
                    "To " & strAtticItem
            Else            
                WScript.StdOut.WriteLine sScriptName & ": Copy Failed    " & vbCrLf & _
                    "From " & vArg & vbcrlf & _
                    "To " & strAtticItem
            End If
        End If
        
         
    End If

End Function


Function RJ(strInput,length,padchar)
    RJ = Right(Replace(Space(length), " ", Left(padchar,1)) & strInput,length)
End Function

Function ISOWeekNum(dtmDate)
  ' Returns a WeekNumber from a date
  Dim NearThurs
  NearThurs = ((dtmDate+5) \ 7) * 7 - 2
  ISOWeekNum = ((NearThurs - DateSerial(Year(NearThurs), 1, 1)) \ 7) + 1
End function
