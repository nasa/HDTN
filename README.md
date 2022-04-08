High Speed Delay Tolerant Network 
==================================

Overview
=========
 Delay Tolerant Networking (or DTN) has been identified as a key technology to facilitate the development and growth of future space networks. Existing DTN implementations have emphasized operation in constrained environments with relatively limited resources and low data speeds. As various technologies have advanced, however, both the rate and efficiency with which data can be effectively transferred have grown incredibly quickly. This has left existing craft unable to utilize more than a small fraction of available capacity. Further, to date, most known implementations of DTN have been designed to operate on the spacecraft themselves. HDTN takes advantage of modern hardware platforms to offer substantial improvement on latency and throughput with respect to DTN implementations that exist today. Specifically, our implementation maintains compatibility with existing implementations of DTN that conform to IETF RFC 5050, while simultaneously defining a new data format that is better suited to higher-rate operation in many cases. It defines and adopts a massively parallel, pipelined, and message oriented architecture, which allows the system to scale gracefully as the resources available to the system increase. HDTN's architecture additionally supports hooks for replacing various elements of the processing pipeline with specialized hardware accelerators, which can be used to offer improved Size, Weight, and Power (SWaP) characteristics at the cost of increased development complexity and cost.

Build Environment 
==================

## Dependencies ## 
HDTN build environment requires:
* CMake version 3.16.3
* Boost library version 1.67.0 minimum, version 1.69.0 for TCPCLv4 TLS version 1.3 support
* ZeroMQ 
* Python module for ZeroMQ
* gcc version 9.3.0 (Debian 8.3.0-6) 
* Target: x86_64-linux-gnu 
* Tested platforms: Ubuntu 20.04.2 LTS, Debian 10, Windows 10 (Visual Studio 2017) and Mac 

## Optional CivetWeb Dependencies ## 
HDTN build environment optionally requires the CivetWeb Embedded C/C++ Web Server Library for displaying real time HDTN data if and only if running in non-distributed mode (using hdtn-one-process executable only).
* Can be obtained from https://github.com/civetweb/civetweb
* User must build its C and C++ libraries using cmake with websocket support enabled.
* Set the Cmake cache variable USE_HDTN_GUI to On (default is Off)
* Set the Cmake variables to point to your civetweb installation: civetweb_INCLUDE, civetweb_LIB, civetwebcpp_LIB

## Optional X86 Hardware Acceleration ## 
HDTN build environment sets by default two CMake cache variables to "On": USE_X86_HARDWARE_ACCELERATION and LTP_RNG_USE_RDSEED.
* If you are building natively (i.e. not cross-compiling) then the HDTN CMake build environment will check the processor's CPU instruction set
as well as the compiler to determine which HDTN hardware accelerated functions will both build and run on the native host.  CMake will automatically set various
compiler defines to enable any supported HDTN hardware accelerated features.
* If you are cross-compiling then the HDTN CMake build environment will check the compiler only to determine if the HDTN hardware accelerated functions will build.
It is up to the user to determine if the target processor will support/run those instructons. CMake will automatically set various
compiler defines to enable any supported HDTN hardware accelerated features if and only if they compile.
* Hardware accelerated functions can be turned off by setting USE_X86_HARDWARE_ACCELERATION and/or LTP_RNG_USE_RDSEED to "Off" in the CMakeCache.txt.
* If you are building for ARM or any non X86-64 platform, USE_X86_HARDWARE_ACCELERATION and LTP_RNG_USE_RDSEED must be set to "Off".

If USE_X86_HARDWARE_ACCELERATION is turned on, then some or all of the following various features will be enabled if CMake finds support for these CPU instructions:
* Fast SDNV encode/decode (BPv6, TCPCLv3, and LTP) requires SSE, SSE2, SSE3, SSSE3, SSE4.1, POPCNT, BMI1, and BMI2.
* Fast batch 32-byte SDNV decode (not yet implemented into HDTN but available in the common/util/Sdnv library) requires AVX, AVX2, and the above "Fast SDNV" support.
* Fast CBOR encode/decode (BPv7) requires SSE and SSE2.
* Some optimized loads and stores for TCPCLv4 requires SSE and SSE2.
* Fast CRC32C (BPv7 and a storage hash function) requires SSE4.2.
* The HDTN storage controller will use BITTEST if available.  If BITTEST is not available, it will use ANDN if BMI1 is available.

If LTP_RNG_USE_RDSEED is turned on, this feature will be enabled if CMake finds support for this CPU instruction:
* An additional source of randomness for LTP's random number generator requires RDSEED.  You may choose to disable this feature for potentially faster LTP performance even if the CPU supports it.

## Packages installation ## 
* Ubuntu
```
$ sudo apt-get install cmake
$ sudo apt-get install build-essential
$ sudo apt-get install libzmq3-dev python3-zmq
$ sudo apt-get install libboost-dev libboost-all-dev
$ sudo apt-get install openssl libssl-dev
```

## Known issue ##
* Ubuntu distributions may install an older CMake version that is not compatible
* Some processors may not support hardware acceleration or the RDSEED instruction, both ON by default in the cmake file

Getting Started
===============

Build HDTN
===========
To build HDTN in Release mode (which is now the default if -DCMAKE_BUILD_TYPE is not specified), follow these steps:
* export HDTN_SOURCE_ROOT=/home/username/hdtn
* cd $HDTN_SOURCE_ROOT
* mkdir build
* cd build
* cmake -DCMAKE_BUILD_TYPE=Release ..
* Edit CMakeCache.txt, set BUILD_SHARED_LIBS:BOOL=[ON|OFF], set Boost_USE_STATIC_LIBS:UNINITIALIZED=[ON|OFF], set CMAKE_INSTALL_PREFIX
* cmake -DCMAKE_BUILD_TYPE=Release ..
* make -j8
* make install

Run HDTN
=========
Note: Ensure your config files are correct, e.g., The outduct remotePort is the same as the induct boundPort, a consistant convergenceLayer, and the outducts remoteHostname is pointed to the correct IP adress.

You can use tcpdump to test the HDTN ingress storage and egress. The generated pcap file can be read using wireshark. 
* sudo tcpdump -i lo -vv -s0 port 4558 -w hdtn-traffic.pcap

In another terminal, run:
* ./runscript.sh

Note: The contact Plan which has a list of all forthcoming contacts for each node is located under module/scheduler/src/contactPlan.json and includes source/destination nodes, start/end time and data rate. Based on the schedule in the contactPlan the scheduler sends events on link availability to Ingress and Storage. When the Ingress receives Link Available event for a given destination, it sends the bundles directly to egress and when the Link is Unavailable it sends the bundles to storage. Upon receiving Link Available event, Storage releases the bundles for the corresponding destination  and when receiving Link Available event it stops releasing the budles. 

There are additional test scripts located under the directories test_scripts_linux and test_scripts_windows that can be used to test different scenarios for all convergence layers.   

Run Unit Tests
===============
After building HDTN (see above), the unit tests can be run with the following command within the build directory:
* ./tests/unit_tests/unit-tests

Run Integrated Tests
====================
After building HDTN (see above), the integrated tests can be run with the following command within the build directory:
* ./tests/integrated_tests/integrated-tests

Run Contact Graph Routing
=========================
HDTN relies on PyCGR for its contact graph routing. We need to specify the location of the contactPlan file as argument. 
To enable the PyCGR Client, in a separate window run:
* python3 ./pycgr/py_cgr_client.py -c module/scheduler/src/contactPlan.json

Routing
=======
The Router module communicates with the CGR client to get the next hop for the optimal route leading to the final destination,
then sends a RouteUpdate event to Egress to update its Outduct to the outduct of that nextHop. If the link goes down
unexpectedly or the contact plan gets updated, the router will be notified and will recalculate the next hop and send
RouteUpdate events to egress accordingly. 
An example of Routing scenario test with 4 HDTN nodes was added under $HDTN_SOURCE_ROOT/test_scripts_linux/Routing_Test

TLS Support for TCPCL Version 4
=======
TLS Versions 1.2 and 1.3 are supported for the TCPCL Version 4 convergence layer.  The X.509 certificates must be version 3 in order to validate IPN URIs using the X.509 "Subject Alternative Name" field.  HDTN must be compiled with ENABLE_OPENSSL_SUPPORT turned on in CMake.
To generate (using a single command) a certificate (which is installed on both an outduct and an induct) and a private key (which is installed on an induct only), such that the induct has a Node Id of 10, use the following command:
* openssl req -x509 -newkey rsa:4096 -nodes -keyout privatekey.pem -out cert.pem -sha256 -days 365 -extensions v3_req -extensions v3_ca -subj "/C=US/ST=Ohio/L=Cleveland/O=NASA/OU=HDTN/CN=localhost" -addext "subjectAltName = otherName:1.3.6.1.5.5.7.8.11;IA5:ipn:10.0" -config /path/to/openssl.cnf

Note: RFC 9174 changed from the earlier -26 draft in that the Subject Alternative Name changed from a URI to an otherName with ID 1.3.6.1.5.5.7.8.11 (id-on-bundleEID).
* Therefore, do NOT use: -addext "subjectAltName = URI:ipn:10.0"
* Instead, use: -addext "subjectAltName = otherName:1.3.6.1.5.5.7.8.11;IA5:ipn:10.0"

To generate the Diffie-Hellman parameters PEM file (which is installed on an induct only), use the following command:
* openssl dhparam -outform PEM -out dh4096.pem 4096

Web User Interface
=========
This repository comes equiped with code to launch a web-based user interface to display statistics for the HDTN engine. However it relies on a dependency called CivetWeb which must be installed.
CivetWeb can be found here: https://github.com/civetweb/civetweb
Follow the installation instructions. 
Then open the CMakeLists.txt file in the hdtn directory and make the following edits under the "USE_HDTN_GUI" section:
* Set "USE_HDTN_GUI" to ON
* Move the CivetServer.h and civetweb.h files to the civetweb_INCLUDE location based on whether the operating system is Windows or Linux/MacOS.
* Move the civetweb.lib file on Windows or libcivetweb.so file on Linux/MacOS to the corresponding civetweb_LIB location.
* If on Windows, move the civetweb-cpp.lib file to the civetwebcpp_LIB location.

Now anytime that HDTNOneProcess is ran, the web page will be accessible at http://localhost:8086

