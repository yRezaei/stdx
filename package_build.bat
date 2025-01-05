@echo off
:: Batch script to clean, create, and build stdx package
:: Usage: package_create.bat <shared|static> <Debug|Release>

:: Input validation
if "%1"=="" (
    echo "Error: Missing first argument. Specify 'shared' or 'static'."
    exit /b 1
)

if "%2"=="" (
    echo "Error: Missing second argument. Specify 'Debug' or 'Release'."
    exit /b 1
)

set SHARED_OPTION=%1
set BUILD_TYPE=%2

:: Convert shared/static option to boolean
if /I "%SHARED_OPTION%"=="shared" (
    set SHARED_FLAG=True
) else if /I "%SHARED_OPTION%"=="static" (
    set SHARED_FLAG=False
) else (
    echo "Error: Invalid first argument. Must be 'shared' or 'static'."
    exit /b 1
)

:: Validate build type
if /I NOT "%BUILD_TYPE%"=="Debug" IF NOT "%BUILD_TYPE%"=="Release" (
    echo "Error: Invalid second argument. Must be 'Debug' or 'Release'."
    exit /b 1
)

:: Check if the stdx package exists in the local cache
echo Checking if stdx package exists in the local cache...
for /f "tokens=*" %%i in ('conan list stdx/0.1@yrezaei/stable -c 2^>^&1') do (
    echo %%i | findstr /C:"Recipe 'stdx' not found" >nul
    if %errorlevel%==0 (
        echo Package stdx/0.1@yrezaei/stable does not exist. Skipping removal.
        goto CREATE_PACKAGE
    )
)

:: If the package exists, remove it
echo Package stdx/0.1@yrezaei/stable exists. Removing...
conan remove stdx -c
if errorlevel 1 (
    echo "Error: Failed to remove existing stdx packages."
    exit /b 1
)

:CREATE_PACKAGE
:: Create the package with specified options
echo Creating and building the stdx package...
conan create . --name=stdx --version=0.1 --user=yrezaei --channel=stable --build=missing -s build_type=%BUILD_TYPE% -o stdx/*:shared=%SHARED_FLAG%
if errorlevel 1 (
    echo "Error: Failed to create and build the stdx package."
    exit /b 1
)

:: Success message
echo "Package created and built successfully!"
exit /b 0
