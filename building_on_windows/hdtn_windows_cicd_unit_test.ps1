# @file hdtn_windows_cicd_unit_test.ps1
# @author  Brian Tomko <brian.j.tomko@nasa.gov>
#
# @copyright Copyright © 2021 United States Government as represented by
# the National Aeronautics and Space Administration.
# No copyright is claimed in the United States under Title 17, U.S.Code.
# All Other Rights Reserved.
#
# @section LICENSE
# Released under the NASA Open Source Agreement (NOSA)
# See LICENSE.md in the source root directory for more information.
#
# @section DESCRIPTION
#
# This script builds all HDTN dependencies for Visual Studio 2022 in x64 Release mode.
# The script then builds HDTN and runs the unit tests.
# The script is designed to be the only call to a Github Action for regression testing HDTN on Windows.
# The script will install built dependencies and built hdtn to HDTN_SOURCE_ROOT/build


#Per Github actions:
#Hardware specification for Windows and Linux virtual machines:
#2-core CPU (x86_64)
#7 GB of RAM
#14 GB of SSD space

$ErrorActionPreference = "Stop" #stop script at first exception

#sanity check to make sure scripts can fail
#cmd.exe /c "$PSScriptRoot\test_fail.bat"
#if($LastExitCode -ne 0) { throw 'test_fail.bat successfully failed' }

#if perl is not in the path (required to build openssl) then fail immediately
perl --version

#if 7-zip is not installed then fail immediately (required to unzip source packages)
set-alias sz "$env:ProgramFiles\7-Zip\7z.exe"
sz -h > $null

#configurable variables
#-----------------------------------
#------build directory-----------------
$build_directory = "C:\hdtn_build" #"${PSScriptRoot}\..\build" #don't install within source, causes cmake issues when building/installing hdtn
#------build machine-----------------
$num_cpu_cores = (Get-ComputerInfo).CsNumberOfLogicalProcessors.ToString()
#------openssl-----------------
$nasm_version = "2.15.05" #required for compiling openssl
$openssl_src_directory = "openssl-1.1.1q"
$openssl_install_directory_name = "${openssl_src_directory}_msvc2022"
#------civetweb-----------------
$civetweb_version = "1.15"
$civetweb_install_directory_name = "civetweb_v${civetweb_version}_msvc2022"
#------zero mq-----------------
$zmq_version = "4.3.4"
$zmq_install_directory_name = "libzmq_msvc2022_x64"
#------boost-----------------
$boost_major_version = "1"
$boost_minor_version = "78"
$boost_patch_level = "0"
$boost_version_dot_separated = "${boost_major_version}.${boost_minor_version}.${boost_patch_level}"
$boost_version_underscore_separated = "${boost_major_version}_${boost_minor_version}_${boost_patch_level}"
$boost_src_directory_name = "boost_${boost_version_underscore_separated}"
$boost_7z_file_name = "${boost_src_directory_name}.7z"
$boost_install_directory_name = "${boost_src_directory_name}_build"
$boost_library_install_prefix = "lib64_msvc2022"
#------hdtn-----------------
$hdtn_install_directory_name = "hdtn_msvc2022_x64"
$Env:HDTN_SOURCE_ROOT = Convert-Path( Resolve-Path -Path "${PSScriptRoot}\..") #simplify the path to get rid of any ..

Write-Output "building with ${num_cpu_cores} logical cpu cores"

#this script should be located in HDTN_SOURCE_ROOT/building_on_windows
if (Test-Path -Path ${build_directory}) {
    throw "${build_directory} already exists!  It must not exist for this script to work."
}
New-Item -ItemType Directory -Force -Path ${build_directory}
$build_directory = Convert-Path( Resolve-Path -Path ${build_directory}) #simplify the path (after it exists from previous line) to get rid of any ..
Write-Output ${build_directory}
push-location ${build_directory}

#download all files first in case a link fails
(new-object System.Net.WebClient).DownloadFile( "https://boostorg.jfrog.io/artifactory/main/release/${boost_version_dot_separated}/source/${boost_7z_file_name}", "${pwd}\${boost_7z_file_name}")
(new-object System.Net.WebClient).DownloadFile( "https://github.com/zeromq/libzmq/archive/refs/tags/v${zmq_version}.zip", "${pwd}\libzmq.zip")
(new-object System.Net.WebClient).DownloadFile( "https://www.nasm.us/pub/nasm/releasebuilds/${nasm_version}/win64/nasm-${nasm_version}-win64.zip", "${pwd}\nasm.zip")
(new-object System.Net.WebClient).DownloadFile( "https://www.openssl.org/source/${openssl_src_directory}.tar.gz", "${pwd}\openssl.tar.gz")
(new-object System.Net.WebClient).DownloadFile( "https://github.com/civetweb/civetweb/archive/refs/tags/v${civetweb_version}.zip", "${pwd}\v${civetweb_version}.zip")




#build boost
sz x ${boost_7z_file_name} > $null
push-location ".\${boost_src_directory_name}"
cmd.exe /c "$PSScriptRoot\build_boost.bat ${num_cpu_cores}"
if($LastExitCode -ne 0) { throw 'Boost failed to build' }
New-Item -ItemType Directory -Force -Path "..\${boost_install_directory_name}"
Move-Item -Path ".\stage\lib" -Destination "..\${boost_install_directory_name}\${boost_library_install_prefix}"
Move-Item -Path ".\boost" -Destination "..\${boost_install_directory_name}\boost"
Pop-Location
Remove-Item -Recurse -Force ${boost_src_directory_name}
Remove-Item ${boost_7z_file_name}

#build libzmq
sz x "libzmq.zip" > $null
New-Item -ItemType Directory -Force -Path ".\libzmq-${zmq_version}\mybuild"
push-location ".\libzmq-${zmq_version}\mybuild"
$zmq_cmake_build_options = ("`" " + #literal quote needed to make this one .bat parameter
    "-DBUILD_SHARED:BOOL=ON " + 
    "-DBUILD_STATIC:BOOL=ON " + 
    "-DBUILD_TESTS:BOOL=OFF " + 
    "-DCMAKE_INSTALL_PREFIX:PATH=`"${PWD}\myinstall`" " + #myinstall is within the mybuild directory, no need for double double quote for .bat quote escape char
    ".." + # .. indicates location of source directory which is up one level (currently in the mybuild directory)
    "`"") #ending literal quote needed to make this one .bat parameter
cmd.exe /c "$PSScriptRoot\build_generic_cmake_project.bat ${num_cpu_cores} ${zmq_cmake_build_options}"
if($LastExitCode -ne 0) { throw 'ZeroMQ failed to build' }
Move-Item -Path ".\myinstall" -Destination "..\..\${zmq_install_directory_name}"
Pop-Location
Remove-Item -Recurse -Force "libzmq-${zmq_version}"
Remove-Item "libzmq.zip"

#extract nasm (an openssl building requirement)
sz x "nasm.zip" > $null
$old_env_path_without_nasm = $Env:Path
$Env:Path = "${pwd}\nasm-${nasm_version}" + [IO.Path]::PathSeparator + $Env:Path #prepend to make it first priority
Write-Output $Env:Path
nasm --version #fail if nasm not working
Remove-Item "nasm.zip"

#build openssl
sz x "openssl.tar.gz" > $null
Remove-Item "openssl.tar.gz"
sz x "openssl.tar" > $null
Remove-Item "openssl.tar"
push-location $openssl_src_directory
cmd.exe /c "$PSScriptRoot\build_openssl.bat ${openssl_install_directory_name}"
if($LastExitCode -ne 0) { throw 'Openssl failed to build' }
Move-Item -Path ".\${openssl_install_directory_name}" -Destination "..\${openssl_install_directory_name}"
Pop-Location
Remove-Item -Recurse -Force $openssl_src_directory
#Remove-Item -Recurse -Force "${openssl_install_directory_name}\html" #not needed if openssl is installed with "make install_sw" instead of "make install"

#remove nasm after openssl build
$Env:Path = $old_env_path_without_nasm

Remove-Item -Recurse -Force ".\nasm-${nasm_version}"

#build civetweb
sz x "v${civetweb_version}.zip" > $null
New-Item -ItemType Directory -Force -Path ".\civetweb-${civetweb_version}\mybuild"
push-location ".\civetweb-${civetweb_version}\mybuild"
$civetweb_cmake_build_options = ("`" " + #literal quote needed to make this one .bat parameter
    "-DCIVETWEB_ENABLE_CXX:BOOL=ON " + 
    "-DCIVETWEB_ENABLE_WEBSOCKETS:BOOL=ON " + 
    "-DCIVETWEB_BUILD_TESTING:BOOL=OFF " + 
    "-DCIVETWEB_INSTALL_EXECUTABLE:BOOL=OFF " +
    "-DCMAKE_INSTALL_PREFIX:PATH=`"${PWD}\myinstall`" " + #myinstall is within the mybuild directory, no need for double double quote for .bat quote escape char
    ".." + # .. indicates location of source directory which is up one level (currently in the mybuild directory)
    "`"") #ending literal quote needed to make this one .bat parameter
cmd.exe /c "$PSScriptRoot\build_generic_cmake_project.bat ${num_cpu_cores} ${civetweb_cmake_build_options}"
if($LastExitCode -ne 0) { throw 'Civetweb failed to build' }
Move-Item -Path ".\myinstall" -Destination "..\..\${civetweb_install_directory_name}"
Pop-Location
Remove-Item -Recurse -Force "civetweb-${civetweb_version}"
Remove-Item -Recurse -Force "${civetweb_install_directory_name}\bin" #contains nothing useful, just msvc runtime dll's
Remove-Item "v${civetweb_version}.zip"


#build hdtn
New-Item -ItemType Directory -Force -Path ".\hdtn-build-tmp"
push-location ".\hdtn-build-tmp"
$hdtn_cmake_build_options = ("`" " + #literal quote needed to make this one .bat parameter
    "-DBOOST_INCLUDEDIR:PATH=`"${build_directory}\${boost_install_directory_name}`" " + 
    "-DBOOST_LIBRARYDIR:PATH=`"${build_directory}\${boost_install_directory_name}\${boost_library_install_prefix}`" " + 
    "-DBOOST_ROOT:PATH=`"${build_directory}\${boost_install_directory_name}`" " + 
    "-DBUILD_SHARED_LIBS:BOOL=ON " + 
    "-DENABLE_OPENSSL_SUPPORT:BOOL=ON " + 
    "-DHDTN_TRY_USE_CPP17:BOOL=ON " + 
    "-DLOG_TO_CONSOLE:BOOL=ON " + 
    "-DLOG_TO_ERROR_FILE:BOOL=OFF " + 
    "-DLOG_TO_MODULE_FILES:BOOL=OFF " + 
    "-DLOG_TO_PROCESS_FILE:BOOL=OFF " + 
    "-DLOG_TO_SINGLE_FILE:BOOL=OFF " + 
    "-DLOG_TO_SUBPROCESS_FILES:BOOL=OFF " + 
    "-DLTP_ENGINE_ALLOW_SAME_ENGINE_TRANSFERS:BOOL=OFF " + 
    "-DLTP_RNG_USE_RDSEED:BOOL=ON " + 
    "-DOPENSSL_ROOT_DIR:PATH=`"${build_directory}\${openssl_install_directory_name}`" " + 
    "-DOPENSSL_USE_STATIC_LIBS:BOOL=OFF " + 
    "-DUSE_WEB_INTERFACE:BOOL=ON " + 
    "-DUSE_X86_HARDWARE_ACCELERATION:BOOL=ON " +
    "-Dcivetweb_INCLUDE:PATH=`"${build_directory}\${civetweb_install_directory_name}\include`" " + 
    "-Dcivetweb_LIB:FILEPATH=`"${build_directory}\${civetweb_install_directory_name}\lib\civetweb.lib`" " + 
    "-Dcivetwebcpp_LIB:FILEPATH=`"${build_directory}\${civetweb_install_directory_name}\lib\civetweb-cpp.lib`" " + 
    "-Dlibzmq_INCLUDE:PATH=`"${build_directory}\${zmq_install_directory_name}\include`" " + 
    "-Dlibzmq_LIB:FILEPATH=`"${build_directory}\${zmq_install_directory_name}\lib\libzmq-v143-mt-4_3_4.lib`" " + 
    "-DCMAKE_INSTALL_PREFIX:PATH=`"${build_directory}\${hdtn_install_directory_name}`" " + #myinstall is within the mybuild directory, no need for double double quote for .bat quote escape char
    "`"${Env:HDTN_SOURCE_ROOT}`"" + 
    "`"") #ending literal quote needed to make this one .bat parameter
cmd.exe /c "$PSScriptRoot\build_generic_cmake_project.bat ${num_cpu_cores} ${hdtn_cmake_build_options}"
if($LastExitCode -ne 0) { throw 'HDTN failed to build' }
Pop-Location
Remove-Item -Recurse -Force "hdtn-build-tmp"

#run hdtn unit tests
$Env:Path = ( #add the shared .dll files to the path
    "${build_directory}\${zmq_install_directory_name}\bin" + [IO.Path]::PathSeparator + 
    "${build_directory}\${boost_install_directory_name}\${boost_library_install_prefix}" + [IO.Path]::PathSeparator + 
    "${build_directory}\${openssl_install_directory_name}\bin" + [IO.Path]::PathSeparator + 
    "${build_directory}\${hdtn_install_directory_name}\lib" + [IO.Path]::PathSeparator + 
    $Env:Path) #prepend to make it first priority
Write-Output $Env:Path
#If you want PowerShell to interpret the string as a command name then use the call operator (&) like so:
& "${build_directory}\${hdtn_install_directory_name}\bin\unit-tests.exe"
if($LastExitCode -ne 0) { throw 'HDTN unit tests failed' }
