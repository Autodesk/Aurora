#!/usr/bin/env groovy
@Library('PSL@master') _

///////////////////////////////////////////////////
// Global constants
COMMONSHELL = new ors.utils.CommonShell(steps, env)

node('forgeogs-win2019-gpu') {
  try {
    stage ("Checkout Windows") {
        checkoutGit()
    }
    stage ("Build Windows") {
        windowsBuild()
    }
    stage ('Test Windows') {
        windowsTest()
    }
  }
  finally {
    // Clear the repo
    deleteDir()
  }
}

///////////////////////////////////////////////////
// Checkout Functions

def checkoutGit() {
  String branch = scm.branches[0].toString()
  withGit {
    COMMONSHELL.shell """
                      git clone -b ${branch} --recurse-submodules https://git.autodesk.com/GFX/Aurora .
                      git lfs pull
                      """
  }
}

///////////////////////////////////////////////////
// Build functions

def windowsBuild() {
    def EXTERNALS_DIR="c:\\jenkins\\workspace\\AuroraExternals"
  bat """
    :: Set up EXTERNALS_DIR
    if not exist ${EXTERNALS_DIR} call mkdir ${EXTERNALS_DIR}

    :: Set up nasm
    if not exist ${EXTERNALS_DIR}\\nasm-2.15.05-win64.zip call curl -o ${EXTERNALS_DIR}\\nasm-2.15.05-win64.zip https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/win64/nasm-2.15.05-win64.zip
    if not exist ${EXTERNALS_DIR}\\nasm-2.15.05 call 7z x -y -aoa ${EXTERNALS_DIR}\\nasm-2.15.05-win64.zip -o${EXTERNALS_DIR}
    call set PATH=${EXTERNALS_DIR}\\nasm-2.15.05;%PATH%

    :: Set up Pyside6/PyOpenGL
    call python -m pip install --upgrade pip
    call python -m pip install PySide6
    call python -m pip install PyOpenGL

    :: Set up Vulkan SDK
    :: We need to install Vulkan SDK silently. For more details please refer to https://vulkan.lunarg.com/doc/view/latest/windows/getting_started.html
    if not exist ${EXTERNALS_DIR}\\VulkanSDK-1.3.231.1-Installer.exe call curl -o ${EXTERNALS_DIR}\\VulkanSDK-1.3.231.1-Installer.exe https://sdk.lunarg.com/sdk/download/1.3.231.1/windows/VulkanSDK-1.3.231.1-Installer.exe
    if not exist "C:\\VulkanSDK\\1.3.231.1\\bin" call ${EXTERNALS_DIR}\\VulkanSDK-1.3.231.1-Installer.exe --accept-licenses --default-answer --confirm-command install
    call set PATH=C:\\VulkanSDK\\1.3.231.1\\bin;%PATH%
    call set VK_SDK_PATH=C:\\VulkanSDK\\1.3.231.1
    call set VULKAN_SDK=C:\\VulkanSDK\\1.3.231.1
 
    :: Set up Visual Studio 2019 Environment
    call C:\\"Program Files (x86)"\\"Microsoft Visual Studio"\\2019\\Enterprise\\VC\\Auxiliary\\Build\\vcvars64.bat

    :: insatll externals
    python -u Scripts\\installExternals.py ${EXTERNALS_DIR}

    :: build Aurora
    echo Configure CMake project
    if exist Build\\CMakeCache.txt del /f Build\\CMakeCache.txt
    :: Windows SDK version 10.0.22000.194 or later is required in https://git.autodesk.com/GFX/Aurora#windows.
    :: We'd disable DX BACKEND as installing the windows sdk maybe have some bad effects on other jobs.
    cmake -S . -B Build -D CMAKE_BUILD_TYPE=Release -D EXTERNALS_DIR=${EXTERNALS_DIR} -DENABLE_DIRECTX_BACKEND=OFF
    cmake --build Build --config Release
    if not errorlevel 0 (
        echo ERROR: Failed to build the project for Release binaries, see console output for details
        exit /b %errorlevel%
    )
    """
}

///////////////////////////////////////////////////
// Test functions

def windowsTest() {
  bat """
    :: Start unit test
    echo Starting OGS modernization unit test for Release
    setlocal EnableDelayedExpansion
    pushd Build\\bin\\Release
    set exit_code=0
    for %%i in (.\\*Tests.exe) do (
        set exe=%%i
        set exe_name=!exe:~2,-4!
        !exe!

        if not errorlevel 0 (
            echo ERROR: Test !exe_name! exited with non-zero exit code, see console output for details
            set exit_code=1
        )
    )
    popd
    :: Break further execution when test fails
    if %exit_code% equ 1 goto end
    echo Done All tests for Release
    :end
    exit /b %exit_code%
    """
}