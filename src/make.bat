@echo off
REM Set the name of the output executable
set OUTPUT_FILE=cycharm.exe

REM Set the name of the source file
set SOURCE_FILE=main.c

REM Compilation command
gcc -o %OUTPUT_FILE% %SOURCE_FILE% -mwindows -lcomdlg32 -lcomctl32

REM Check if compilation was successful
if %ERRORLEVEL% EQU 0 (
    echo Compilation successful.
) else (
    echo Compilation failed!
)

