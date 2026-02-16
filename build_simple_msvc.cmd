@echo off
setlocal

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

where cl >nul 2>nul
if errorlevel 1 (
    call :init_msvc
    if errorlevel 1 exit /b 1
)

cl /nologo /W4 /D_CRT_SECURE_NO_WARNINGS /std:c11 ^
 "%ROOT%\main.c" "%ROOT%\game.c" "%ROOT%\io.c" "%ROOT%\save.c" ^
 /Fe:"%ROOT%\simple_main.exe"

if errorlevel 1 exit /b 1

echo Build success: %ROOT%\simple_main.exe
exit /b 0

:init_msvc
set "VSDEVCMD=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEVCMD%" set "VSDEVCMD=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEVCMD%" set "VSDEVCMD=C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEVCMD%" set "VSDEVCMD=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEVCMD%" (
    echo Error: VsDevCmd.bat not found.
    echo Install Visual Studio Build Tools 2022 with C++ workload.
    exit /b 1
)

call "%VSDEVCMD%" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 (
    echo Error: failed to initialize MSVC environment.
    exit /b 1
)

where cl >nul 2>nul
if errorlevel 1 (
    echo Error: cl.exe is still unavailable after VsDevCmd.
    exit /b 1
)
exit /b 0
