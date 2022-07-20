@echo off
::
:: start and stop the StorageTek 4400 simulator
::
if /I "%1"=="start" start "STK4400" /D ..\stk npm start
if /I "%1"=="stop"  taskkill /FI "WINDOWTITLE eq STK4400" /F
