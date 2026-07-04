@echo off
::
:: start and stop the web terminal server
::
if /I "%1"=="start" (
	start "TPM 0" /D webterm cmd /c node ..\..\webterm\viking-console -p 6602 -r 6604 -i vike1.pid
	start "TPM 1" /D webterm cmd /c node ..\..\webterm\viking-console -p 6603 -r 6605 -i vike2.pid
)
if /I "%1"=="stop"  (
	for /F %%p in (webterm\vike1.pid) do taskkill /pid %%p /F /T
	for /F %%p in (webterm\vike2.pid) do taskkill /pid %%p /F /T
)
