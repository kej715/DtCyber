@echo off
::
:: start and stop the web terminal server
::
if /I "%1"=="start" start "Console-Server" /D webterm cmd /c node ..\..\webterm\webterm-server -p console.pid console.json
if /I "%1"=="stop"  (
	for /F %%p in (webterm\console.pid) do taskkill /pid %%p /F /T
)
