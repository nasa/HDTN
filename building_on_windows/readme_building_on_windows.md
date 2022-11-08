HDTN On Windows
==================================

## Overview ##
 HDTN has been tested on 64-bit editions of Windows 10, Windows Server 2022, and Windows Server 2019.  HDTN supports 9 permutations of the Visual Studio compilers on Windows:
 * Versions: 2022, 2019, and 2017 (note: for 2017, only versions 15.7 and 15.9 have been testes)
 * Editions: Enterprise, Professional, and Community

## Build Requirements ##
HDTN build environment on Windows requires:
* One of the supported Visual Studio compilers listed in the Overview section.  Visual Studio must be installed for C++ Development during setup.
* PowerShell (recommended Visual Studio Code with the PowerShell extension installed)
* 7-Zip
* Perl (needed for building OpenSSL) with perl.exe in the Path environmental variable (Strawberry Perl for Windows has been tested)


## Build HDTN and its Dependencies (non-development) ##
To build HDTN and its dependencies in Release mode and as shared libraries (shared .dll files for both HDTN and its dependencies), simply run the PowerShell script in `building_on_windows\hdtn_windows_cicd_unit_test.ps1` from any working directory.  The working directory does not matter.  Once finished, HDTN and its dependencies will be installed to `C:\hdtn_build_x64_release_vs2022` (suffix will be 2019 or 2017 if that's the Visual Studio compiler installed).  The script will also run HDTN's unit-tests after the build.  Once completed, you will see the following message:

`"Remember, HDTN was built as a shared library, so you must prepend the following to your Path so that Windows can find the DLL's of HDTN and its dependencies:"`

It will print out four directory locations to add to your Path environmental variable in order to be able to use HDTN outside of this PowerShell script.
* From the Windows Start Menu, type "env", open "Edit environmental variables for your account"
* double click Path
* Add the four directories. (Omit the directory containing `hdtn_install\lib` if you are going to modify HDTN source code within Visual Studio.  You will build and install your own HDTN binaries later within Visual Studio.)
* If you are a user of HDTN and you are NOT going to modify HDTN source code within Visual Studio, also add this directory to your Path: `C:\hdtn_build_x64_release_vs2022\hdtn_install\bin`
* Click `OK`
* Click `New`
* Add the following new variable: `HDTN_SOURCE_ROOT`
* Set the variable value to your source root (the folder that contains README.md).  Example `C:\path\to\hdtn`
* Click `OK`
* Click `OK`

If you are a user of HDTN and you are NOT going to modify HDTN source code within Visual Studio, you can reference any of the .bat file example tests located in `HDTN_SOURCE_ROOT\tests\test_scripts_windows`.  Note that these scripts were intended for developers, so you will have to modify the scripts, fixing any lines that reference `HDTN_BUILD_ROOT`, so, for example, if you see `"%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe"`, replace it with `bpgen-async.exe`.  Also note that these .bat files reference config files located in `HDTN_SOURCE_ROOT\config_files`, so feel free to modify those .json configs to meet your needs.


If you are a developer and you are going to modify HDTN source code within Visual Studio, you may delete the directory `C:\hdtn_build_x64_release_vs2022\hdtn_install` and continue on with the next set of instructions.

## Setup Instructions for HDTN Developers ##
First complete the previous step "Build HDTN and its Dependencies."  Then, perform the following steps:

Launch Visual Studio 2022 and open HDTN as a project with these steps:
* File >> open >> cmake
* Open HDTN root CMakeLists.txt
* Make sure drop down configuration at the top is set to x64-Release.  You may need to go to `Manage Configurations` if not.

Then click `Project >> view CMakeCache.txt`
Add these lines (change `_vs2022` directory suffix if different):
* `BOOST_INCLUDEDIR:PATH=C:\hdtn_build_x64_release_vs2022\boost_1_78_0_install`
* `BOOST_LIBRARYDIR:PATH=C:\hdtn_build_x64_release_vs2022\boost_1_78_0_install\lib64`
* `BOOST_ROOT:PATH=C:\hdtn_build_x64_release_vs2022\boost_1_78_0_install`
* `OPENSSL_INCLUDE_DIR:PATH=C:\hdtn_build_x64_release_vs2022\openssl-1.1.1q_install\include`
* `OPENSSL_ROOT_DIR:PATH=C:\hdtn_build_x64_release_vs2022\openssl-1.1.1q_install`
* `libzmq_INCLUDE:PATH=C:\hdtn_build_x64_release_vs2022\libzmq_v4.3.4_install\include`
* `libzmq_LIB:FILEPATH=C:\hdtn_build_x64_release_vs2022\libzmq_v4.3.4_install\lib\libzmq-v143-mt-4_3_4.lib` (note: may be v141 or v142)
* `civetweb_INCLUDE:PATH=C:\hdtn_build_x64_release_vs2022\civetweb_v1.15_install\include`
* `civetweb_LIB:FILEPATH=C:\hdtn_build_x64_release_vs2022\civetweb_v1.15_install\lib\civetweb.lib`
* `civetwebcpp_LIB:FILEPATH=C:\hdtn_build_x64_release_vs2022\civetweb_v1.15_install\lib\civetweb-cpp.lib`
* `BUILD_SHARED_LIBS:BOOL=ON`
* `USE_WEB_INTERFACE:BOOL=ON`

Then click `Project >> configure cache`

It's now time to set up additional environmental variables in order to be able to run the .bat file tests located in `HDTN_SOURCE_ROOT\tests\test_scripts_windows`:
* Right click on the open tab within Visual Studio titled `CMakeCache.txt` and then click "Open Containing Folder"
* Copy the path at the top of the Windows Explorer window
* From the Windows Start Menu, type "env", open "Edit environmental variables for your account"
* Click `New`
* Add the following new variable: `HDTN_BUILD_ROOT`.  The variable value will look something like `C:\Users\username\CMakeBuilds\17e7ec0d-5e2f-4956-8a91-1b32467252b0\build\x64-Release`
* Click `OK`
* Click `New`
* Add the following new variable: `HDTN_INSTALL_ROOT`.  The variable value will look similar to `HDTN_BUILD_ROOT` except change "build" to "install".. something like `C:\Users\username\CMakeBuilds\17e7ec0d-5e2f-4956-8a91-1b32467252b0\install\x64-Release`
* Click `OK`
* double click `Path` variable, add the `HDTN_INSTALL_ROOT\lib` folder to your `Path`.. something like `C:\Users\username\CMakeBuilds\17e7ec0d-5e2f-4956-8a91-1b32467252b0\install\x64-Release\lib`.  This step is needed because HDTN is built as a shared library with multiple .dll files, so this step allows Windows to find those .dll files when running any HDTN binaries.

Relaunch Visual studio so that it get's loaded with updated environmental variables.  Now build HDTN:
* Build >> Build All
* Build >> Install HDTN
* Run `unit_tests.bat` located in `HDTN_SOURCE_ROOT\tests\test_scripts_windows`
* For a Web GUI example, run `test_tcpcl_fast_cutthrough_oneprocess.bat` and then navigate to http://localhost:8086/web_gui.html (note: to exit cleanly, do a ctrl-c in each cmd window before closing)

Important: Since CMake is currently configured to build HDTN as a shared library (because the CMake cache variable `BUILD_SHARED_LIBS` is set to `ON`), any time you make a source code change to HDTN, for it to be reflected in the binaries, don't forget to `Build >> Install HDTN` after the `Build >> Build All` step.

## Setup Instructions for Developers Using Installed HDTN Libraries within their own Projects ##
HDTN utilizes modern CMake.  When HDTN is installed, it installs the appropriate CMake Packages that can be imported.  For an example of this use case, see `HDTN_SOURCE_ROOT\tests\unit_tests_import_installation\CMakeLists.txt` for a project that imports the libraries and headers from an HDTN installation and builds HDTN's unit tests from that installation.
