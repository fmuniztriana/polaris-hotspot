@echo off
setlocal
cd /d "%~dp0\.."
where cl >nul 2>nul
if errorlevel 1 (
  echo Run from an x64 Visual Studio Developer Command Prompt.
  exit /b 1
)
if not exist build mkdir build
rc /nologo /I windows /fo build\PolarisHotspot.res windows\PolarisHotspot.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /W4 /EHsc /O2 /MT /Iinclude windows\main.cpp build\PolarisHotspot.res /Fo:build\hotspot.obj /Fe:build\PolarisHotspot.exe /link /SUBSYSTEM:WINDOWS advapi32.lib user32.lib gdi32.lib setupapi.lib shell32.lib bcrypt.lib uuid.lib /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
exit /b %errorlevel%
