@echo off
::
:: start and stop the web terminal server
::
if /I "%1"=="start" start "WebTerm-Server" /D webterm cmd /c node ..\..\webterm\webterm-server -p webterm.pid config.json
if /I "%1"=="stop"  (
	for /F %%p in (webterm.pid) do taskkill /pid %%p /F /T
)
