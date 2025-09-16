@echo off
echo Building Lag DLL...

REM 设置Visual Studio 2022路径
set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
set "MSBUILD_PATH=%VS_PATH%\MSBuild\Current\Bin\MSBuild.exe"

REM 检查MSBuild是否存在
if exist "%MSBUILD_PATH%" (
    echo Using MSBuild from Visual Studio 2022...
    "%MSBUILD_PATH%" TrafficMonitorPlugins.sln /p:Configuration=Release /p:Platform=x64 /t:Lag
    if %errorlevel% equ 0 (
        echo Build successful!
        goto :end
    ) else (
        echo Build failed with error code %errorlevel%
        goto :end
    )
)

REM 尝试使用PATH中的MSBuild
where msbuild >nul 2>&1
if %errorlevel% equ 0 (
    echo Using MSBuild from PATH...
    msbuild TrafficMonitorPlugins.sln /p:Configuration=Release /p:Platform=x64 /t:Lag
    goto :end
)

REM 如果都找不到，提示用户
echo Error: MSBuild not found
echo Please install Visual Studio 2022 or run this from Visual Studio Developer Command Prompt

:end
echo Build completed. Check Bin\x64\Release\ for the DLL file.
pause
