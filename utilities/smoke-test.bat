@echo off
REM Reproducible smoke-test checklist for memory/stability audit (Windows).
setlocal
set ROOT=%~dp0..
set BUILD_DIR=%BUILD_DIR%
if "%BUILD_DIR%"=="" set BUILD_DIR=build-asan
set REPORT=%ROOT%\utilities\audit-baseline.txt

echo Source-X memory audit smoke test> "%REPORT%"
echo Build dir: %BUILD_DIR%>> "%REPORT%"
echo.>> "%REPORT%"

if exist "%BUILD_DIR%" (
    ctest --test-dir "%BUILD_DIR%" --output-on-failure >> "%REPORT%" 2>&1
) else (
    echo Build directory missing. Run utilities\build-asan-windows.bat first.>> "%REPORT%"
)

echo.>> "%REPORT%"
echo Manual sanitizer scenarios:>> "%REPORT%"
echo   1. Startup + script load + player login>> "%REPORT%"
echo   2. Create/destroy items, containers, multis>> "%REPORT%"
echo   3. NPC vendor trade>> "%REPORT%"
echo   4. World save + GarbageCollection>> "%REPORT%"
echo   5. Client disconnect during combat/targeting>> "%REPORT%"
echo.>> "%REPORT%"
echo Report written to %REPORT%
