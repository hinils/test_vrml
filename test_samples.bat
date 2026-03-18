@echo off
chcp 65001 >nul
set VIEWER=C:\Users\tomcat\Desktop\vrml_viewer\build\Release\vrml_viewer.exe
set SAMPLES=C:\Users\tomcat\Desktop\vrml_viewer\samples

echo ========================================
echo   VRML Samples Test Script
echo ========================================
echo.

for %%f in ("%SAMPLES%\*.wrl" "%SAMPLES%\*.vrml") do (
    echo.
    echo [Testing] %%~nxf
    echo ----------------------------------------
    "%VIEWER%" "%%f"
    echo.
    echo Completed: %%~nxf
    echo.
    choice /c YN /m "Continue to next file? (Y/N)"
    if errorlevel 2 goto :end
)

:end
echo.
echo All tests completed!
pause
