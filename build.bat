@echo off
setlocal

rem ============================================================
rem  Macro Trainer build script
rem ============================================================

rem --- Step 1: Close Geometry Dash if running ---------------------------
rem  Windows locks the .geode file while GD has it loaded into memory,
rem  so we must close GD before we can overwrite the mod. tasklist
rem  doesn't fail if GD isn't running — taskkill might, so we check first.
tasklist /fi "imagename eq GeometryDash.exe" 2>nul | find /i "GeometryDash.exe" >nul
if %errorlevel% equ 0 (
    echo Geometry Dash is running. Closing it...
    taskkill /im GeometryDash.exe /f >nul 2>&1
    rem Give Windows a moment to release the file handles.
    timeout /t 1 /nobreak >nul
)

rem --- Step 2: Activate MSVC 14.44 toolchain (CMake doesn't like 14.50) -
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44 >nul
if errorlevel 1 (
    echo Failed to load vcvars.
    pause
    exit /b 1
)

rem --- Step 3: Configure CMake (only if build/ doesn't exist) -----------
if not exist build (
    echo Configuring CMake...
    cmake -B build -G Ninja ^
        -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
        -DCMAKE_C_COMPILER=clang ^
        -DCMAKE_CXX_COMPILER=clang++
    if errorlevel 1 (
        echo CMake configure failed.
        pause
        exit /b 1
    )
)

rem --- Step 4: Build ----------------------------------------------------
echo Building...
cmake --build build
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

rem --- Step 5: Copy .geode into GD's mods folder ------------------------
echo Copying to GD mods folder...
copy /Y build\reparked.gmdsync.geode "C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\geode\mods\" >nul
if errorlevel 1 (
    echo Copy failed.
    pause
    exit /b 1
)

echo.
echo === Done. You can launch Geometry Dash now. ===
echo.

pause
endlocal