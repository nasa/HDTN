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
# This script builds all HDTN dependencies for Visual Studio (versions 2022, 2019, or 2017) in x64 Release mode (shared libs).
# The script locates all versions of Visual Studio installed on the machine,
# choosing either the $vc_preferred_version variable (if installed) or the latest installed version 
# with the highest license (i.e. Enterprise first, then Professional, then Community).
# The script then builds HDTN and runs the unit tests.
# The script is designed to be the only call to a Github Action for regression testing HDTN on Windows.
# The script will install built dependencies and built hdtn to "C:\hdtn_build_x64_release_vs[2017,2019, or 2022]""
# The script prints out at the end what needs prepended to the Windows "Path" environmental variable.

#Per Github actions:
#Hardware specification for Windows and Linux virtual machines:
#2-core CPU (x86_64)
#7 GB of RAM
#14 GB of SSD space

$ErrorActionPreference = "Stop" #stop script at first exception



#if perl is not in the path (required to build openssl) then fail immediately
perl --version

#if 7-zip is not installed then fail immediately (required to unzip source packages)
if (Test-Path -Path "${env:ProgramFiles}\7-Zip\7z.exe") {
    set-alias sz "${env:ProgramFiles}\7-Zip\7z.exe"
}
elseif (Test-Path -Path "${env:ProgramFiles(x86)}\7-Zip\7z.exe") {
    set-alias sz "${env:ProgramFiles(x86)}\7-Zip\7z.exe"
}
else {
    throw "Cannot find an installed 7-Zip!"
}
sz -h > $null


#------visual studio vcvars64 possible locations-----------------
$vcvars_end_path = "VC\Auxiliary\Build\vcvars64.bat"
$vc_preferred_version = "2022"
$vc_versions="2022","2019","2017"
$vc_editions="Enterprise","Professional","Community"
$vc_program_files="${env:ProgramFiles}","${env:ProgramFiles(x86)}"
$vc_found_list = @()
foreach ($vc_version in $vc_versions) {
    foreach ($vc_edition in $vc_editions) {
            foreach ($vc_program_file in $vc_program_files) {
            $test_path = "${vc_program_file}\Microsoft Visual Studio\${vc_version}\${vc_edition}\${vcvars_end_path}"
            if (Test-Path -Path ${test_path}) {
                $vc_found_list += ( [System.Tuple]::Create($vc_version, $vc_edition, $vc_program_file, $test_path))
                Write-Output "Found a Visual Studio Path at: ${test_path}"
            }
        }
    }
}

$vc_using_tuple = $null
if ($vc_found_list) {
    foreach ($vc_found in $vc_found_list) {
        if($vc_found.Item1 -eq $vc_preferred_version) {
            $vc_using_tuple = $vc_found
            break
        }
    }
} 
else {
    throw "Could not find an installed Visual Studio product!"
}

if ($null -eq $vc_using_tuple){
    $vc_using_tuple = $vc_found_list[0]
    Write-Output ("The installed Visual Studio ({0} {1}) isn't the preferred ${vc_preferred_version} version." -f $vc_using_tuple.Item1, $vc_using_tuple.Item2)
}


$vc_version = $vc_using_tuple.Item1
$vc_edition = $vc_using_tuple.Item2
$vcvars64_path = $vc_using_tuple.Item4
$vcvars64_path_with_quotes = "`"${vcvars64_path}`"" #literal quote needed to make this one .bat parameter
$cmake_generator_arg = $null
if($vc_version -eq "2022") {
    $cmake_generator_arg = " -G `"`"Visual Studio 17 2022`"`" -A x64 " #double double quotes will be replaced within bat file
}
elseif($vc_version -eq "2019") {
    $cmake_generator_arg = " -G `"`"Visual Studio 16 2019`"`" -A x64 "
}
elseif($vc_version -eq "2017") {
    $cmake_generator_arg = " -G `"`"Visual Studio 15 2017`"`" -A x64 "
}
else {
    throw "error invalid vc_version ${vc_version}."
}
Write-Output "Building using the installed Visual Studio ${vc_version} ${vc_edition}."
Write-Output "Visual Studio vcvars64.bat location: ${vcvars64_path_with_quotes}."
Write-Output "CMake Generator argument: ${cmake_generator_arg}."

#------build machine-----------------
$num_cpu_cores = (Get-ComputerInfo).CsNumberOfLogicalProcessors
if($num_cpu_cores) {
    $num_cpu_cores = $num_cpu_cores.ToString()
    Write-Output "building with ${num_cpu_cores} logical cpu cores"
}
else { #null
    $num_cpu_cores = "2"
    Write-Output "unable to detect num logical cpu cores, defaulting to ${num_cpu_cores}"
}

#configurable variables
#-----------------------------------
#------build directory-----------------
$build_directory_prefix = "C:\hdtn_build" #appends _x64_release_vs[2017,2019, or 2022] #"${PSScriptRoot}\..\build" #don't install within source, causes cmake issues when building/installing hdtn
#------openssl-----------------
$build_openssl_with_jom = $true #parallel build
$jom_version = "1.1.3"
$jom_version_underscore_separated = $jom_version.replace('.','_')
$nasm_version = "2.15.05" #required for compiling openssl
$openssl_version = "1.1.1s"
$openssl_src_directory = "openssl-${openssl_version}"
$openssl_install_directory_name = "${openssl_src_directory}_install"
#------zero mq-----------------
$zmq_version = "4.3.4"
$zmq_version_underscore_separated = $zmq_version.replace('.','_')
$zmq_install_directory_name = "libzmq_v${zmq_version}_install"
#------boost-----------------
$boost_major_version = "1"
$boost_minor_version = "78"
if($vc_version -eq "2017") {
    $boost_minor_version = "72" #a known version that can be detected with FindBoost in 2017's CMake v3.12
}
$boost_patch_level = "0"
$boost_version_dot_separated = "${boost_major_version}.${boost_minor_version}.${boost_patch_level}"
$boost_version_underscore_separated = "${boost_major_version}_${boost_minor_version}_${boost_patch_level}"
$boost_src_directory_name = "boost_${boost_version_underscore_separated}"
$boost_7z_file_name = "${boost_src_directory_name}.7z"
$boost_install_directory_name = "${boost_src_directory_name}_install"
$boost_library_install_prefix = "lib64"
#------hdtn-----------------
$hdtn_install_directory_name = "hdtn_install"
$Env:HDTN_SOURCE_ROOT = Convert-Path( Resolve-Path -Path "${PSScriptRoot}\..") #simplify the path to get rid of any ..

#sanity check to make sure scripts can fail
#$test_bat_param = "`"$cmake_generator_arg`""
#Write-Output "test_bat_param = ${test_bat_param}"
#cmd.exe /c "$PSScriptRoot\test_fail.bat ${test_bat_param} ${vcvars64_path_with_quotes}"
#if($LastExitCode -ne 0) { throw 'test_fail.bat successfully failed' }
#throw "stop"



#this script should be located in HDTN_SOURCE_ROOT/building_on_windows

$build_directory = "${build_directory_prefix}_x64_release_vs${vc_version}"
if (Test-Path -Path ${build_directory}) {
    Write-Output "${build_directory} already exists."
}
else {
    New-Item -ItemType Directory -Force -Path ${build_directory}
}

$build_directory = Convert-Path( Resolve-Path -Path ${build_directory}) #simplify the path (after it exists from previous line) to get rid of any ..
Write-Output ${build_directory}
push-location ${build_directory}

$zmq_is_installed = (Test-Path -Path "${build_directory}\${zmq_install_directory_name}\bin")
$boost_is_installed = (Test-Path -Path "${build_directory}\${boost_install_directory_name}\${boost_library_install_prefix}")
$openssl_is_installed = (Test-Path -Path "${build_directory}\${openssl_install_directory_name}\bin")
$hdtn_is_installed = (Test-Path -Path "${build_directory}\${hdtn_install_directory_name}\lib")


#download all files first in case a link fails
if($boost_is_installed) {
    Write-Output "Boost already installed."
}
else {
    if(Test-Path -Path "${pwd}\${boost_7z_file_name}") {
        Write-Output "${pwd}\${boost_7z_file_name} already exists, not downloading"
    }
    else {
        $boost_download_link = "https://boostorg.jfrog.io/artifactory/main/release/${boost_version_dot_separated}/source/${boost_7z_file_name}"
        Write-Output "Downloading Boost from ${boost_download_link}"
        (new-object System.Net.WebClient).DownloadFile($boost_download_link, "${pwd}\${boost_7z_file_name}")
    }
}

if($zmq_is_installed) {
    Write-Output "ZeroMQ already installed."
}
else {
    if(Test-Path -Path "${pwd}\libzmq.zip") {
        Write-Output "${pwd}\libzmq.zip already exists, not downloading"
    }
    else {
        $zmq_download_link = "https://github.com/zeromq/libzmq/archive/refs/tags/v${zmq_version}.zip"
        Write-Output "Downloading ZeroMQ from ${zmq_download_link}"
        (new-object System.Net.WebClient).DownloadFile($zmq_download_link, "${pwd}\libzmq.zip")
    }
}

if($openssl_is_installed) {
    Write-Output "OpenSSL already installed."
}
else {
    if(Test-Path -Path "${pwd}\nasm.zip") {
        Write-Output "${pwd}\nasm.zip already exists, not downloading"
    }
    else {
        $nasm_download_link = "https://www.nasm.us/pub/nasm/releasebuilds/${nasm_version}/win64/nasm-${nasm_version}-win64.zip"
        Write-Output "Downloading NASM from ${nasm_download_link}"
        (new-object System.Net.WebClient).DownloadFile($nasm_download_link, "${pwd}\nasm.zip")
    }

    if($build_openssl_with_jom) {
        if(Test-Path -Path "${pwd}\jom.zip") {
            Write-Output "${pwd}\jom.zip already exists, not downloading"
        }
        else {
            $jom_download_link = "https://download.qt.io/official_releases/jom/jom_${jom_version_underscore_separated}.zip"
            Write-Output "Downloading JOM from ${jom_download_link}"
            (new-object System.Net.WebClient).DownloadFile($jom_download_link, "${pwd}\jom.zip")
        }
    }

    if(Test-Path -Path "${pwd}\openssl.tar.gz") {
        Write-Output "${pwd}\openssl.tar.gz already exists, not downloading"
    }
    else {
        $openssl_download_link = "https://www.openssl.org/source/${openssl_src_directory}.tar.gz"
        Write-Output "Downloading OpenSSL from ${openssl_download_link}"
        (new-object System.Net.WebClient).DownloadFile($openssl_download_link, "${pwd}\openssl.tar.gz")
    }
}




#build boost
if(-Not $boost_is_installed) {
    if(Test-Path -Path ${boost_src_directory_name}) {
        Remove-Item -Recurse -Force ${boost_src_directory_name}
    }
    sz x ${boost_7z_file_name} > $null
    push-location ".\${boost_src_directory_name}"
    cmd.exe /c "$PSScriptRoot\build_boost.bat ${num_cpu_cores} ${vcvars64_path_with_quotes}"
    if($LastExitCode -ne 0) { throw 'Boost failed to build' }
    New-Item -ItemType Directory -Force -Path "..\${boost_install_directory_name}"
    Move-Item -Path ".\stage\lib" -Destination "..\${boost_install_directory_name}\${boost_library_install_prefix}"
    Move-Item -Path ".\boost" -Destination "..\${boost_install_directory_name}\boost"
    Pop-Location
    Remove-Item -Recurse -Force ${boost_src_directory_name}
    Remove-Item ${boost_7z_file_name}
}

#build libzmq
if(-Not $zmq_is_installed) {
    if(Test-Path -Path "libzmq-${zmq_version}") {
        Remove-Item -Recurse -Force "libzmq-${zmq_version}"
    }
    sz x "libzmq.zip" > $null
    New-Item -ItemType Directory -Force -Path ".\libzmq-${zmq_version}\mybuild"
    push-location ".\libzmq-${zmq_version}\mybuild"
    $zmq_cmake_build_options = ("`" " + #literal quote needed to make this one .bat parameter
        "${cmake_generator_arg} " +
        "-DBUILD_SHARED:BOOL=ON " + 
        "-DBUILD_STATIC:BOOL=ON " + 
        "-DBUILD_TESTS:BOOL=OFF " + 
        "-DCMAKE_INSTALL_PREFIX:PATH=`"`"${PWD}\myinstall`"`" " + #myinstall is within the mybuild directory, double double quotes for .bat quote escape char
        ".." + # .. indicates location of source directory which is up one level (currently in the mybuild directory)
        "`"") #ending literal quote needed to make this one .bat parameter
    cmd.exe /c "$PSScriptRoot\build_generic_cmake_project.bat ${num_cpu_cores} ${zmq_cmake_build_options} ${vcvars64_path_with_quotes}"
    if($LastExitCode -ne 0) { throw 'ZeroMQ failed to build' }
    Move-Item -Path ".\myinstall" -Destination "..\..\${zmq_install_directory_name}"
    Pop-Location
    Remove-Item -Recurse -Force "libzmq-${zmq_version}"
    Remove-Item "libzmq.zip"
}

if(-Not $openssl_is_installed) {
    #extract nasm (an openssl building requirement)
    if(Test-Path -Path ".\nasm-${nasm_version}") {
        Remove-Item -Recurse -Force ".\nasm-${nasm_version}"
    }
    sz x "nasm.zip" > $null
    $old_env_path_without_nasm = $Env:Path
    $Env:Path = "${pwd}\nasm-${nasm_version}" + [IO.Path]::PathSeparator + $Env:Path #prepend to make it first priority
    Write-Output $Env:Path
    nasm --version #fail if nasm not working
    Remove-Item "nasm.zip"

    $openssl_make_command = "`"nmake`"" #literal quote needed to make this one .bat parameter
    if($build_openssl_with_jom) {
        #extract jom (an optional openssl building requirement)
        if(Test-Path -Path ".\jom") {
            Remove-Item -Recurse -Force ".\jom"
        }
        sz x -ojom "jom.zip" > $null
        #old env path saved above in $old_env_path_without_nasm = $Env:Path
        $Env:Path = "${pwd}\jom" + [IO.Path]::PathSeparator + $Env:Path #prepend to make it first priority
        Write-Output $Env:Path
        jom /VERSION #fail if jom not working
        Remove-Item "jom.zip"
        $openssl_make_command = "`"jom /J ${num_cpu_cores}`""
    }

    #build openssl
    if(Test-Path -Path $openssl_src_directory) {
        Remove-Item -Recurse -Force $openssl_src_directory
    }
    sz x "openssl.tar.gz" > $null
    Remove-Item "openssl.tar.gz"
    sz x "openssl.tar" > $null
    Remove-Item "openssl.tar"
    push-location $openssl_src_directory
    cmd.exe /c "$PSScriptRoot\build_openssl.bat ${openssl_install_directory_name} ${vcvars64_path_with_quotes} ${openssl_make_command}"
    if($LastExitCode -ne 0) { throw 'Openssl failed to build' }
    Move-Item -Path ".\${openssl_install_directory_name}" -Destination "..\${openssl_install_directory_name}"
    Pop-Location
    Remove-Item -Recurse -Force $openssl_src_directory
    #Remove-Item -Recurse -Force "${openssl_install_directory_name}\html" #not needed if openssl is installed with "make install_sw" instead of "make install"

    #remove jom after openssl build
    if(Test-Path -Path ".\jom") {
        Remove-Item -Recurse -Force ".\jom"
    }

    #remove nasm after openssl build
    $Env:Path = $old_env_path_without_nasm

    Remove-Item -Recurse -Force ".\nasm-${nasm_version}"
}


#locate installed libzmq_LIB
$libzmq_lib_filepath = $null
For ($i=0; $i -le 9; $i++) {
    $libzmq_lib_filepath_test = "${build_directory}\${zmq_install_directory_name}\lib\libzmq-v14${i}-mt-${zmq_version_underscore_separated}.lib"
    if(Test-Path -Path $libzmq_lib_filepath_test) {
        $libzmq_lib_filepath = $libzmq_lib_filepath_test
        break
    }
}
if($libzmq_lib_filepath) {
    Write-Output "Found libzmq_LIB at ${libzmq_lib_filepath}"
}
else {
    throw "Cannot find libzmq_LIB"
}

#build hdtn
if(-Not $hdtn_is_installed) {
    if(Test-Path -Path ".\hdtn-build-tmp") {
        Remove-Item -Recurse -Force "hdtn-build-tmp"
    }
    New-Item -ItemType Directory -Force -Path ".\hdtn-build-tmp"
    push-location ".\hdtn-build-tmp"
    $TRY_USE_CPP17 = "ON"
    if($vc_version -eq "2017") {
        $TRY_USE_CPP17 = "OFF" #too many deprecation warnings printed
    }
    $hdtn_cmake_build_options = ("`" " + #literal quote needed to make this one .bat parameter
        "${cmake_generator_arg} " +
        "-DBOOST_INCLUDEDIR:PATH=`"`"${build_directory}\${boost_install_directory_name}`"`" " + 
        "-DBOOST_LIBRARYDIR:PATH=`"`"${build_directory}\${boost_install_directory_name}\${boost_library_install_prefix}`"`" " + 
        "-DBOOST_ROOT:PATH=`"`"${build_directory}\${boost_install_directory_name}`"`" " + 
        "-DBUILD_SHARED_LIBS:BOOL=ON " + 
        "-DENABLE_OPENSSL_SUPPORT:BOOL=ON " + 
        "-DHDTN_TRY_USE_CPP17:BOOL=${TRY_USE_CPP17} " + 
        "-DLOG_TO_CONSOLE:BOOL=ON " + 
        "-DLOG_TO_ERROR_FILE:BOOL=OFF " + 
        "-DLOG_TO_MODULE_FILES:BOOL=OFF " + 
        "-DLOG_TO_PROCESS_FILE:BOOL=OFF " + 
        "-DLOG_TO_SINGLE_FILE:BOOL=OFF " + 
        "-DLOG_TO_SUBPROCESS_FILES:BOOL=OFF " + 
        "-DLTP_ENGINE_ALLOW_SAME_ENGINE_TRANSFERS:BOOL=OFF " + 
        "-DLTP_RNG_USE_RDSEED:BOOL=ON " + 
        "-DOPENSSL_ROOT_DIR:PATH=`"`"${build_directory}\${openssl_install_directory_name}`"`" " + 
        "-DOPENSSL_USE_STATIC_LIBS:BOOL=OFF " + 
        "-DUSE_WEB_INTERFACE:BOOL=ON " + 
        "-DUSE_X86_HARDWARE_ACCELERATION:BOOL=ON " +
        "-Dlibzmq_INCLUDE:PATH=`"`"${build_directory}\${zmq_install_directory_name}\include`"`" " + 
        "-Dlibzmq_LIB:FILEPATH=`"`"${libzmq_lib_filepath}`"`" " + 
        "-DCMAKE_INSTALL_PREFIX:PATH=`"`"${build_directory}\${hdtn_install_directory_name}`"`" " + #myinstall is within the mybuild directory, double double quote for .bat quote escape char
        "`"`"${Env:HDTN_SOURCE_ROOT}`"`"" + 
        "`"") #ending literal quote needed to make this one .bat parameter
    cmd.exe /c "$PSScriptRoot\build_generic_cmake_project.bat ${num_cpu_cores} ${hdtn_cmake_build_options} ${vcvars64_path_with_quotes}"
    if($LastExitCode -ne 0) {
        Start-Sleep -Seconds 2
        # Get-ChildItem -Recurse
        if(Test-Path -Path ".\CMakeFiles\CMakeError.log") {
            Write-Output "here is the content of .\CMakeFiles\CMakeError.log:"
            get-content ".\CMakeFiles\CMakeError.log"
        }
        if(Test-Path -Path ".\CMakeFiles\CMakeConfigureLog.yaml") {
            Write-Output "here is the content of .\CMakeFiles\CMakeConfigureLog.yaml:"
            get-content ".\CMakeFiles\CMakeConfigureLog.yaml"
        }

        throw 'HDTN failed to build'
    }
    Pop-Location
    Remove-Item -Recurse -Force "hdtn-build-tmp"
}
#run hdtn unit tests
$to_prepend_to_path = ( #add the shared .dll files to the path
    "${build_directory}\${zmq_install_directory_name}\bin" + [IO.Path]::PathSeparator + 
    "${build_directory}\${boost_install_directory_name}\${boost_library_install_prefix}" + [IO.Path]::PathSeparator + 
    "${build_directory}\${openssl_install_directory_name}\bin" + [IO.Path]::PathSeparator + 
    "${build_directory}\${hdtn_install_directory_name}\lib" + [IO.Path]::PathSeparator)
$Env:Path = $to_prepend_to_path + $Env:Path #prepend to make it first priority
Write-Output "HDTN was built as a shared library, so you must prepend the following to your Path so that Windows can find the DLL's of HDTN and its dependencies:"
Write-Output "${to_prepend_to_path}"
Write-Output ""
Write-Output "About to run unit tests..."
#If you want PowerShell to interpret the string as a command name then use the call operator (&) like so:
& "${build_directory}\${hdtn_install_directory_name}\bin\unit-tests.exe"
if($LastExitCode -ne 0) { throw 'HDTN unit tests failed' }
Write-Output "HDTN's unit tests were successful!"
Write-Output "Remember, HDTN was built as a shared library, so you must prepend the following to your Path so that Windows can find the DLL's of HDTN and its dependencies:"
Write-Output "${to_prepend_to_path}"
