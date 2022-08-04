@echo off
::
:: start and stop the StorageTek 4400 simulator
::
if /I "%1"=="start" start "STK4400" /D ..\stk cmd /c node server
if /I "%1"=="stop"  (
	for /F %%p in (..\stk\pid) do taskkill /pid %%p /F /T
)
