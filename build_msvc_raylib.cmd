@echo off
setlocal

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "RAYLIB_ROOT=%ROOT%\third_party\raylib-5.5_win64_msvc16\raylib-5.5_win64_msvc16"
set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%VCVARS%" (
    echo Error: vcvars64.bat not found at:
    echo   %VCVARS%
    exit /b 1
)

if not exist "%RAYLIB_ROOT%\include\raylib.h" (
    echo Error: raylib headers not found at:
    echo   %RAYLIB_ROOT%\include\raylib.h
    exit /b 1
)

if not exist "%RAYLIB_ROOT%\lib\raylib.lib" (
    echo Error: raylib library not found at:
    echo   %RAYLIB_ROOT%\lib\raylib.lib
    exit /b 1
)

call "%VCVARS%" || exit /b 1

cl /Zi /EHsc /MD /DUSE_RAYLIB /nologo /I"%RAYLIB_ROOT%\include" ^
 /Fe"%ROOT%\main.exe" ^
 "%ROOT%\main.c" "%ROOT%\game.c" "%ROOT%\io.c" "%ROOT%\save.c" "%ROOT%\ai.c" "%ROOT%\rayui.c" ^
 /link /LIBPATH:"%RAYLIB_ROOT%\lib" raylib.lib winmm.lib gdi32.lib user32.lib shell32.lib opengl32.lib

if errorlevel 1 exit /b 1

copy /Y "%RAYLIB_ROOT%\lib\raylib.dll" "%ROOT%\raylib.dll" >nul
echo Build success: %ROOT%\main.exe
echo Runtime DLL:   %ROOT%\raylib.dll
exit /b 0
