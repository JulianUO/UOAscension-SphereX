@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM SphereServer (Source-X) - Local Pre-Commit Unit Tests & Validation Runner
REM ============================================================================

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
set "BUILD_DIR=%ROOT_DIR%\build-local-tests"

echo [1/4] Checking build tools and environment...

where cmake.exe >nul 2>nul
if %errorlevel% neq 0 (
    echo CMake not in PATH. Searching Visual Studio installation...
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath 2^>nul`) do (
        set "VS_PATH=%%i"
    )
    if defined VS_PATH (
        echo Found Visual Studio at: !VS_PATH!
        if exist "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat" (
            call "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat" >nul
        ) else if exist "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" (
            call "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
        )
        set "PATH=!VS_PATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;!PATH!"
        set "PATH=!VS_PATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;!PATH!"
    )
)

where cmake.exe >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] cmake.exe not found in PATH or Visual Studio installation.
    exit /b 1
)

echo [2/4] Configuring CMake project with UNIT_TESTING=ON...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -DUNIT_TESTING=ON -DCMAKE_BUILD_TYPE=Nightly
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    exit /b %errorlevel%
)

echo [3/4] Building SphereServer unit tests...
cmake --build "%BUILD_DIR%" --config Nightly
if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    exit /b %errorlevel%
)

echo [4/4] Executing unit tests (CTest)...
ctest --test-dir "%BUILD_DIR%" -C Nightly --output-on-failure
if %errorlevel% neq 0 (
    echo [FAIL] One or more unit tests failed!
    exit /b %errorlevel%
)

echo.
echo ============================================================================
echo [SUCCESS] All local unit tests passed! Ready for commit.
echo ============================================================================
exit /b 0
