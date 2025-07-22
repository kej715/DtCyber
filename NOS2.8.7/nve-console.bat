@echo off
::
:: start and stop the web terminal server
::
if /I "%1"=="start" (
	start "NOS/VE" /D webterm cmd /c node ..\..\webterm\viking-console -i vike1.pid
	start "NOS/VE" /D webterm cmd /c node ..\..\webterm\viking-console -i vike2.pid
)
if /I "%1"=="stop"  (
	for /F %%p in (webterm\vike1.pid) do taskkill /pid %%p /F /T
	for /F %%p in (webterm\vike2.pid) do taskkill /pid %%p /F /T
)
