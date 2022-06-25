Option Explicit

' Syntax:
'  CSCRIPT /nologo getdate.vbs
 
Dim dt
dt=now
'output format: yyyymmddHHnnss

WScript.StdOut.Write Right("0000" & year(dt),4) 
WScript.StdOut.Write Right("00" & month(dt),2) 
WScript.StdOut.Write Right("00" & day(dt),2)
WScript.StdOut.Write Right("00" & Hour(dt),2)
WScript.StdOut.Write Right("00" & Minute(dt),2)
WScript.StdOut.Write Right("00" & Second(dt),2)