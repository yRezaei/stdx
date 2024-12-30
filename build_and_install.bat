@echo off
REM Build and install script for stdx library (run from root folder)

REM Default values
set BUILD_TYPE=Debug
set NUM_JOBS=34

REM Check for custom build type argument
if not "%~1"=="" (
    set BUILD_TYPE=%1
)

REM Set the install directory to include the build type
set INSTALL_DIR=%~dp0bin\%BUILD_TYPE%

REM Display build and install configuration
echo Building with configuration: %BUILD_TYPE%
echo Installation directory: %INSTALL_DIR%

REM Create and navigate to the build directory
set BUILD_DIR=%~dp0build
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
)

REM Run the CMake configuration
echo Configuring the project...
cmake -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%INSTALL_DIR% -S "%~dp0" -B "%BUILD_DIR%"

REM Run the build command
echo Building the project...
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --target ALL_BUILD -j %NUM_JOBS%

REM Run the install command
echo Installing the project...
cmake --install "%BUILD_DIR%" --config %BUILD_TYPE%

REM Indicate completion
echo Build and installation complete.
