@echo off
echo Building Power Lock for all architectures...
echo.

echo ========================================
echo Building x64 version...
echo ========================================
call build-x64.bat
if %errorlevel% neq 0 (
    echo x64 build failed!
    exit /b 1
)

echo.
echo ========================================
echo Building x86 version...
echo ========================================
call build-x86.bat
if %errorlevel% neq 0 (
    echo x86 build failed!
    exit /b 1
)

echo.
echo ========================================
echo Building ARM64 version...
echo ========================================
call build-arm64.bat
if %errorlevel% neq 0 (
    echo ARM64 build failed!
    exit /b 1
)

echo.
echo ========================================
echo All builds completed successfully!
echo ========================================
echo.
echo Built executables:
echo   bin\power_lock_service.exe     (x64)
echo   bin\powerlock.exe              (x64)
echo   bin\x86\power_lock_service.exe (x86)
echo   bin\x86\powerlock.exe          (x86)
echo   bin\arm64\power_lock_service.exe (ARM64)
echo   bin\arm64\powerlock.exe        (ARM64)
echo.
