@echo off
REM Configure and build Source-X with AddressSanitizer (MSVC; VS 2019+).
REM Usage: utilities\build-asan-windows.bat [build_dir]
setlocal
set BUILD_DIR=%~1
if "%BUILD_DIR%"=="" set BUILD_DIR=build-asan

echo ==^> Configuring ASAN build in %BUILD_DIR%
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Windows-MSVC.cmake ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DUSE_ASAN=ON ^
    -B %BUILD_DIR% -S .

if errorlevel 1 exit /b 1

echo ==^> Building
cmake --build %BUILD_DIR% --config Debug
if errorlevel 1 exit /b 1

echo ==^> Done. Configure sanitizer env before running:
echo     utilities\configure-asan.bat
echo     %BUILD_DIR%\bin-x86_64\SphereSvrX64_debug.exe
