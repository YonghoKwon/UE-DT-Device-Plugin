@echo off
rem Prepare (DTCore + DTDevice link + Editor build) and open the DTDevice TestHost.
rem
rem DTCore location, in priority order:
rem   1) Open_TestHost.bat <source> [ref]
rem        source = git URL (internal repo OK) or a local DTCore folder containing DTCore.uplugin
rem        ref    = branch, tag or commit for git sources
rem   2) TestHost.local.ini  ([DTCore] Source= / Ref=, copy TestHost.local.ini.example; not committed)
rem   3) environment variables DTCORE_SOURCE / DTCORE_REF
rem   4) default: public UE-DT-Core-Plugin at the validated commit
setlocal
set "HOST=%~dp0"
set "ENGINE=C:\Program Files\Epic Games\UE_5.3"
if not "%UE_ENGINE_ROOT%"=="" set "ENGINE=%UE_ENGINE_ROOT%"

set "ARGS="
if not "%~1"=="" set ARGS=-DTCoreSource "%~1"
if not "%~2"=="" set ARGS=%ARGS% -DTCoreRef "%~2"

powershell -NoProfile -ExecutionPolicy Bypass -File "%HOST%Scripts\setup_testhost.ps1" -EngineRoot "%ENGINE%" %ARGS%
if errorlevel 1 (
  echo.
  echo TestHost setup failed. See the messages above.
  pause
  exit /b 1
)
start "" "%ENGINE%\Engine\Binaries\Win64\UnrealEditor.exe" "%HOST%DTDeviceTestHost.uproject"
endlocal
