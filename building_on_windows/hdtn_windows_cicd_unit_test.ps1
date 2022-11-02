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

#sanity check to make sure scripts can fail
#cmd.exe /c "$PSScriptRoot\test_fail.bat"
#if($LastExitCode -ne 0) { throw 'test_fail.bat successfully failed' }

#if perl is not in the path (required to build openssl) then fail immediately
perl --version

#if 7-zip is not installed then fail immediately (required to unzip source packages)
set-alias sz "$env:ProgramFiles\7-Zip\7z.exe"
sz -h

#configurable variables
#-----------------------------------
#------build directory-----------------
$build_directory = "${PSScriptRoot}\..\build"
#------build machine-----------------
$num_cpu_cores = "8"
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

#this script should be located in HDTN_SOURCE_ROOT/building_on_windows
New-Item -ItemType Directory -Force -Path ${build_directory}
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
$Env:Path += [IO.Path]::PathSeparator + "$pwd\nasm-${nasm_version}"
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


