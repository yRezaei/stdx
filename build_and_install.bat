@echo off
REM Build and install script for stdx library (run from root folder)

REM Default values
set BUILD_TYPE=Debug

REM Check for custom build type argument
if not "%~1"=="" (
    set BUILD_TYPE=%1
)

REM Set the install directory to the "bin" folder
set INSTALL_DIR=%~dp0bin

REM Display build and install configuration
echo Building with configuration: %BUILD_TYPE%
echo Installation directory: %INSTALL_DIR%

REM Create or clean the build directory
set BUILD_DIR=%~dp0build
if exist "%BUILD_DIR%" (
    echo Cleaning build directory...
    rmdir /s /q "%BUILD_DIR%"
)
mkdir "%BUILD_DIR%"

REM Create or clean the install (bin) directory
if exist "%INSTALL_DIR%" (
    echo Cleaning install directory...
    rmdir /s /q "%INSTALL_DIR%"
)
mkdir "%INSTALL_DIR%"

REM Run the CMake configuration
echo Configuring the project...
cmake -S . -B build ^
      -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
      -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR%"

REM Check if configuration succeeded
if errorlevel 1 (
    echo CMake configuration failed.
    pause
    exit /b 1
)

REM Run the build command
echo Building the project...
cmake --build build --config %BUILD_TYPE%

REM Check if build succeeded
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

REM Run the install command
echo Installing the project...
cmake --install build --config %BUILD_TYPE%

REM Check if installation succeeded
if errorlevel 1 (
    echo Installation failed.
    pause
    exit /b 1
)

REM Indicate completion
echo Build and installation complete.
pause
