@echo off
REM Builds (if needed) and runs the Kaleidoscope. No Visual Studio IDE required.
setlocal
cd /d "%~dp0"
set SRC=Kaleidoscope\Kaleidoscope\main.cpp
set EXE=Kaleidoscope.exe

if exist "%EXE%" goto run

echo Building...
where g++ >nul 2>nul
if %errorlevel%==0 (
  g++ -O2 -static -o "%EXE%" "%SRC%" -lopengl32 -lgdi32 -luser32
  goto check
)

where cl >nul 2>nul
if %errorlevel%==0 (
  cl /nologo /O2 /EHsc /std:c++17 /Fe:"%EXE%" "%SRC%" /link opengl32.lib gdi32.lib user32.lib
  goto check
)

echo No compiler found.
echo Install MinGW-w64 (https://winlibs.com) and add it to PATH, or open
echo "x64 Native Tools Command Prompt for VS" and run this script from there.
pause
exit /b 1

:check
if not exist "%EXE%" (
  echo Build failed.
  pause
  exit /b 1
)

:run
"%EXE%"
