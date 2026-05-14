@echo off
setlocal EnableDelayedExpansion
echo === Building LSPosed Module APK ===

:: Find Android SDK
set "SDK="
if defined ANDROID_HOME set "SDK=%ANDROID_HOME%"
if defined ANDROID_SDK_ROOT set "SDK=%ANDROID_SDK_ROOT%"
if not defined SDK (
    for %%d in (
        "C:\Android\Sdk"
        "%LOCALAPPDATA%\Android\Sdk"
        "C:\Users\%USERNAME%\AppData\Local\Android\Sdk"
    ) do (
        if exist "%%~d\platforms" (
            set "SDK=%%~d"
            goto :sdk_found
        )
    )
)
:sdk_found

if not defined SDK (
    echo ERROR: Android SDK not found.
    echo Set ANDROID_HOME or ANDROID_SDK_ROOT, or install Android Studio.
    exit /b 1
)
echo SDK: %SDK%

:: Write local.properties
echo sdk.dir=%SDK:\=/%> local.properties

:: Build
call gradle assembleRelease
if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed
    exit /b 1
)

:: Output
set "APK=app\build\outputs\apk\release\app-release-unsigned.apk"
if exist "%APK%" (
    echo === Build SUCCEEDED ===
    echo APK: %CD%\%APK%
    echo.
    echo Install to Quest:
    echo   adb install %APK%
    echo.
    echo Then enable in LSPosed Manager and scope to your game.
) else (
    echo WARN: APK not found at expected path: %APK%
)

endlocal
