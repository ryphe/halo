@echo off
setlocal

echo ========================================
echo halo build script
echo ========================================
echo.

:: set include flag if headers directory exists
set "INC_FLAGS="
if exist headers (
    set "INC_FLAGS=/Iheaders"
)

:: 1. build and run the icon generator
cl /O2 /W3 icon.c /Fe:generate_icon.exe
generate_icon.exe halo.ico

:: 2. compile the embedded resources
rc /nologo /fo halo.res halo.rc

:: 3. compile halo with the icon resource linked in
cl /O2 /MD /W3 /fp:fast /std:c17 /D_CRT_SECURE_NO_WARNINGS -Iheaders main.c halo.res /Fe:halo.exe user32.lib gdi32.lib winmm.lib comctl32.lib comdlg32.lib

:: preserves the compilation status before del
set "BUILD_RESULT=%ERRORLEVEL%"

:: optional: clean up intermediate build artifacts
del *.res *.obj 2>nul

if %BUILD_RESULT% EQU 0 (
    echo.
    echo [SUCCESS] Build completed: halo.exe
) else (
    echo.
    echo [ERROR] Compilation failed!
    exit /b 1
)