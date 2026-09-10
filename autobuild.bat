@echo off
setlocal enabledelayedexpansion

rem ================================================================
rem  OrbbecSDK Windows (Win10) auto build script
rem  Windows port of the original linux autobuild.sh, same features:
rem   1. check / create build dir
rem   2. cmake configure + build + install
rem   3. show install dir structure
rem   4. read version / git commit, pack install into zip, md5
rem  Note: for a fresh configure (different generator/arch),
rem        delete the build dir and rerun this script
rem  Note: this file is pure ASCII, safe for any codepage/editor
rem ================================================================

set "BUILD_DIR=build"
set "PKG_ARCH=win_x64"

echo.
echo ================================================================
echo  OrbbecSDK Windows auto build start
echo ================================================================

rem 1. check build dir
if not exist "%BUILD_DIR%" (
    echo ---------------create ^& check build dir------------------------
    mkdir "%BUILD_DIR%"
)

pushd "%BUILD_DIR%"
echo ------------------build start--------------------------------------

rem 2. cmake configure: only when build/CMakeCache.txt does not exist
rem    (VS generator auto re-configures when CMakeLists changes)
rem    note: BUILD_LINUX_ARM64 is a linux cross-compile option,
rem    replaced with -A x64 on Windows for 64-bit
if not exist "CMakeCache.txt" (
    echo ---------------cmake configure---------------------------------
    cmake -DCMAKE_BUILD_TYPE=Release -A x64 ..
    if errorlevel 1 goto :fail
)

echo ---------------cmake build-----------------------------------------
cmake --build . --config Release --parallel
if errorlevel 1 goto :fail

echo ---------------cmake install---------------------------------------
cmake --install . --prefix ./install
if errorlevel 1 goto :fail

rem 3. tree display install
echo ------------------tree install dir---------------------------------
tree install
echo -------------------------------------------------------------------
echo.

rem 4. clean old package files
echo ------------------pack zip install dir------------------------------
for %%f in (*.zip *.md5.txt) do (
    if exist "%%f" del /q "%%f"
)

rem 5. get timestamp (yyyyMMddHHmmss)
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMddHHmmss"') do set "TIMESTR=%%i"

rem 6. get version from CMakeLists.txt project(... VERSION x.x.x.x ...)
rem    default version: 2.9.0.1
set "VERSION=2.9.0.1"
for /f "usebackq tokens=1-4" %%a in (`findstr /b /c:"project(" "..\CMakeLists.txt"`) do (
    if /i "%%b"=="VERSION" (
        echo %%c | findstr /r "^[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$" >nul
        if not errorlevel 1 set "VERSION=%%c"
    )
)
echo version: %VERSION%

rem 7. get git commit id (first 8 chars)
set "COMMIT_ID="
if exist "..\.git" (
    for /f %%c in ('git rev-parse HEAD') do set "COMMIT_ID=%%c"
    if defined COMMIT_ID set "COMMIT_ID=!COMMIT_ID:~0,8!"
    echo commit_id: !COMMIT_ID!
)

rem 8. package name
set "PKG_NAME=OrbbecSDK_v%VERSION%_%TIMESTR%"
if defined COMMIT_ID (
    set "PKG_NAME=OrbbecSDK_v%VERSION%_%TIMESTR%_!COMMIT_ID!_%PKG_ARCH%"
) else (
    set "PKG_NAME=OrbbecSDK_v%VERSION%_%TIMESTR%_%PKG_ARCH%"
)
echo package_name: %PKG_NAME%

rem 9. pack install dir to zip (Win10 built-in PowerShell Compress-Archive)
powershell -NoProfile -Command "Compress-Archive -Path install -DestinationPath '%PKG_NAME%.zip' -Force"
if errorlevel 1 goto :fail

rem 10. generate package md5 (same format as linux md5sum: hash  filename)
set "HASH="
for /f "skip=1 tokens=1" %%h in ('certutil -hashfile "%PKG_NAME%.zip" MD5') do (
    if not defined HASH set "HASH=%%h"
)
> "%PKG_NAME%.md5.txt" echo %HASH%  %PKG_NAME%.zip

echo.
echo ----------------------------------------------------------------------------------------------------
dir "%PKG_NAME%.zip" "%PKG_NAME%.md5.txt"
echo ----------------------------------------------------------------------------------------------------
echo.
popd
echo ---------------build success---------------------------------------
exit /b 0

:fail
echo [ERROR] build failed, see messages above.
popd
exit /b 1
