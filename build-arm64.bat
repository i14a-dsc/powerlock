@echo off
echo Building Power Lock (ARM64)...

if not exist "bin\arm64" mkdir bin\arm64

cl >nul 2>&1
if %errorlevel% neq 0 (
    echo Setting up Visual Studio environment for ARM64...
    call env-arm64.bat
) else (
    echo Visual Studio environment already available.
)

echo Compiling src\power_lock_service.c...
cl /nologo src\power_lock_service.c ^
   /Fe:bin\arm64\power_lock_service.exe ^
   /link user32.lib powrprof.lib advapi32.lib

if %errorlevel% neq 0 (
    echo Build failed for power_lock_service.exe!
    exit /b 1
)

echo Compiling src\powerlock.c...
cl /nologo src\powerlock.c ^
   /Fe:bin\arm64\powerlock.exe ^
   /link advapi32.lib

del *.obj >nul 2>&1

if %errorlevel% equ 0 (
    echo.
    echo Build successful! Created:
    echo   bin\arm64\power_lock_service.exe
    echo   bin\arm64\powerlock.exe
    echo.
) else (
    echo.
    echo Build failed!
    exit /b 1
)

exit /b 0
