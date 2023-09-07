#!/usr/bin/env groovy
@Library('PSL@master') _

///////////////////////////////////////////////////
// Global constants
COMMONSHELL = new ors.utils.CommonShell(steps, env)

node('OGS_Win_002') {
  try {
    stage ("Checkout Windows") {
        checkoutGit()
    }
    stage ("Build Windows Release") {
        windowsBuild("Release")
    }
    stage ("Build Windows Debug") {
        windowsBuild("Debug")
    }
    stage ('Test Windows Release') {
        windowsTest("Release")
    }
    stage ('Test Windows Debug') {
        windowsTest("Debug")
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
  checkout([$class: 'GitSCM',
            branches: scm.branches,
            doGenerateSubmoduleConfigurations: false,
            extensions: [[$class: 'GitLFSPull']],
            submoduleCfg: [],
            userRemoteConfigs: scm.userRemoteConfigs])
}

///////////////////////////////////////////////////
// Build functions

def windowsBuild(Config) {
  def EXTERNALS_DIR="c:\\jenkins\\workspace\\AuroraExternals"
  def EXTERNALS_DIR_AURORA="${EXTERNALS_DIR}\\vs2019\\${Config}"
  bat """
    :: Set up EXTERNALS_DIR
    if not exist ${EXTERNALS_DIR_AURORA} call mkdir ${EXTERNALS_DIR_AURORA}

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
    if not exist "C:\\VulkanSDK\\1.3.231.1\\bin" if not exist ${EXTERNALS_DIR}\\VulkanSDK-1.3.231.1-Installer.exe call curl -o ${EXTERNALS_DIR}\\VulkanSDK-1.3.231.1-Installer.exe https://sdk.lunarg.com/sdk/download/1.3.231.1/windows/VulkanSDK-1.3.231.1-Installer.exe
    if not exist "C:\\VulkanSDK\\1.3.231.1\\bin" call ${EXTERNALS_DIR}\\VulkanSDK-1.3.231.1-Installer.exe --accept-licenses --default-answer --confirm-command install com.lunarg.vulkan.debug
    call set PATH=C:\\VulkanSDK\\1.3.231.1\\bin;%PATH%
    call set VK_SDK_PATH=C:\\VulkanSDK\\1.3.231.1
    call set VULKAN_SDK=C:\\VulkanSDK\\1.3.231.1

    :: Set up Visual Studio 2019 Environment
    for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere" -property installationPath`) do (
        if exist "%%i\\MSBuild\\Microsoft\\VisualStudio\\v16.0" set "VSInstallPath=%%i"
    )
    call "%VSInstallPath%\\VC\\Auxiliary\\Build\\vcvars64.bat"

    :: install externals
    python -u Scripts\\installExternals.py ${EXTERNALS_DIR_AURORA} --build-variant=${Config} -v -v

    :: build Aurora
    echo Configure CMake project
    if exist Build\\CMakeCache.txt del /f Build\\CMakeCache.txt

    :: As the build machine is Windows Server 2019 while target running machine is Windows 10,
    :: Use CMAKE_SYSTEM_VERSION to target the build for a different version of the host operating system than is actually running on the host
    cmake -S . -B Build -D CMAKE_BUILD_TYPE=${Config} -D EXTERNALS_DIR=${EXTERNALS_DIR_AURORA} -DCMAKE_SYSTEM_VERSION=10.0.22000.0  -G "Visual Studio 16 2019" -A x64
    cmake --build Build --config ${Config}
    if not errorlevel 0 (
        echo ERROR: Failed to build the project for ${Config} binaries, see console output for details
        exit /b %errorlevel%
    )
    """
}

///////////////////////////////////////////////////
// Test functions

def windowsTest(Config) {
  bat """
    :: Start unit test
    echo Starting OGS modernization unit test for ${Config}
    setlocal EnableDelayedExpansion
    pushd Build\\bin\\${Config}
    set exit_code=0
    for %%i in (.\\*Tests.exe) do (
        set exe=%%i
        set exe_name=!exe:~2,-4!
        !exe!

        if !errorlevel! neq 0 (
            echo ERROR: Test !exe_name! exited with non-zero exit code, see console output for details
            set exit_code=1
        )
    )
    popd
    :: Break further execution when test fails
    if %exit_code% equ 1 goto end
    echo Done All tests for ${Config}
    :end
    exit /b %exit_code%
    """
}