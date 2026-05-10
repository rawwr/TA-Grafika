@echo off
set BUILD_DIR=projects\cmake\build

if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo Building PBR project...
cmake -S projects\cmake -B %BUILD_DIR% -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build %BUILD_DIR% --target install --config Release

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful! Launching PBR.exe...
    echo.
    cd data
    start "" PBR.exe
    cd ..
) else (
    echo.
    echo Build failed! Please check the error messages above.
    pause
)
