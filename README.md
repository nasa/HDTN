High-rate Delay Tolerant Network 
==================================

Overview
=========
Delay Tolerant Networking (DTN) has been identified as a key technology to facilitate the development and growth of future space networks. DTN is an overlay network that uses the bundle protocol to connect once disparate one-to-one links. Bundles are the primary unit of data in a DTN, and can be of essentially any size. Existing DTN implementations have operated in constrained environments with limited resources resulting in low data speeds, and cannot utilize more than a fraction of available system capacity. However, as various technologies have advanced, data transfer rates and efficiency have also advanced. To date, most known implementations of DTN have been designed to operate on spacecraft.

High-rate Delay Tolerant Networking (HDTN) takes advantage of modern hardware platforms to substantially reduce latency and improve throughput compared to today’s DTN operations. HDTN maintains compatibility with existing deployments of DTN that conform to IETF RFC 5050. At the same time, HDTN defines a new data format better suited to higher-rate operation. It defines and adopts a massively parallel pipelined and message-oriented architecture, allowing the system to scale gracefully as its resources increase. HDTN’s architecture also supports hooks to replace various processing pipeline elements with specialized hardware accelerators. This offers improved Size, Weight, and Power (SWaP) characteristics while reducing development complexity and cost.

Also see the [HDTN wiki](https://github.com/nasa/HDTN/wiki) for more information. 

Architecture
=============
HDTN is written in C++, and is designed to be modular. These modules include:
* Ingress - Processes incoming bundles.
* Scheduler - Determines if outgoing bundles can be forwarded or must be stored based on the contact plan.
* Storage - Stores bundles to disk.
* Router - Calculates the next hop for the bundle.
* Egress - Forwards bundles to the proper outduct and next hop.
* Telemetry Command Interface - Web interface that displays the operations and data for HDTN.

Build Environment
==================
## Tested Platforms ##
* Linux
    * Ubuntu 20.04.2 LTS
    * Debian 10
    * RHEL (Red Hat Enterprise Linux) 8
* Windows
    * Windows 10 (64-bit)
    * Windows Server 2022 (64-bit)
    * Windows Server 2019 (64-bit)
* Raspbian
* ARM on x86

## Dependencies ## 
HDTN build environment requires:
* CMake version 3.12 minimum
* Boost library version 1.66.0 minimum, version 1.69.0 for TCPCLv4 TLS version 1.3 support
* ZeroMQ (tested with version 4.3.4)
* OpenSSL (recommended version 1.1.1).  If OpenSSL is not available, disable OpenSSL support via the CMake cache variable `ENABLE_OPENSSL_SUPPORT:BOOL`
* On Linux: gcc version 9.3.0 (Debian 8.3.0-6)
* On Windows: see the [Windows Build Instructions](building_on_windows/readme_building_on_windows.md)
* Target: x86_64-linux-gnu 
* Tested platforms: Ubuntu 20.04.2 LTS, Debian 10, Windows 10 (Visual Studio), Windows Server 2019 (Visual Studio), Windows Server 2022 (Visual Studio), and Mac

These can be installed on Linux with:
* On Ubuntu
```
sudo apt-get install cmake build-essential libzmq3-dev libboost-dev libboost-all-dev openssl libssl-dev
```
* On RHel
```
sudo dnf install epel-release
sudo yum install cmake boost-devel zeromq zeromq-devel
```
## Known issue ##
* Ubuntu distributions may install an older CMake version that is not compatible
* Some processors may not support hardware acceleration or the RDSEED instruction, both ON by default in the cmake file


## Notes on C++ Standard Version ##
HDTN build environment sets by default the CMake cache variable `HDTN_TRY_USE_CPP17` to `On`.
* If set to `On`, CMake will test if the compiler supports C++17.  If the test fails, CMake will fall back to C++11 and compile HDTN with C++11.  If the test passes, CMake will compile HDTN with C++17.
* If set to `Off`, CMake will compile HDTN with C++11.
* With Visual Studio 2017 version 15.7 and later versions, if C++17 is enabled, CMake will set the compile option `/Zc:__cplusplus` to make the `__cplusplus` preprocessor macro accurate for Visual Studio.  (Otherwise, without this compile option, by default, Visual Studio always returns the value `199711L` for the `__cplusplus` preprocessor macro.)
* Throughout the HDTN codebase, C++17 optimizations exist, usually within a `#if(__cplusplus >= 201703L)` block.

## Optional X86 Hardware Acceleration ## 
HDTN build environment sets by default two CMake cache variables to `On`: `USE_X86_HARDWARE_ACCELERATION` and `LTP_RNG_USE_RDSEED`.
* If you are building natively (i.e. not cross-compiling) then the HDTN CMake build environment will check the processor's CPU instruction set
as well as the compiler to determine which HDTN hardware accelerated functions will both build and run on the native host.  CMake will automatically set various
compiler defines to enable any supported HDTN hardware accelerated features.
* If you are cross-compiling then the HDTN CMake build environment will check the compiler only to determine if the HDTN hardware accelerated functions will build.
It is up to the user to determine if the target processor will support/run those instructons. CMake will automatically set various
compiler defines to enable any supported HDTN hardware accelerated features if and only if they compile.
* Hardware accelerated functions can be turned off by setting `USE_X86_HARDWARE_ACCELERATION` and/or `LTP_RNG_USE_RDSEED` to `Off` in the `CMakeCache.txt`.
* If you are building for ARM or any non X86-64 platform, `USE_X86_HARDWARE_ACCELERATION` and `LTP_RNG_USE_RDSEED` must be set to `Off`.

If `USE_X86_HARDWARE_ACCELERATION` is turned on, then some or all of the following various features will be enabled if CMake finds support for these CPU instructions:
* Fast SDNV encode/decode (BPv6, TCPCLv3, and LTP) requires SSE, SSE2, SSE3, SSSE3, SSE4.1, POPCNT, BMI1, and BMI2.
* Fast batch 32-byte SDNV decode (not yet implemented into HDTN but available in the common/util/Sdnv library) requires AVX, AVX2, and the above "Fast SDNV" support.
* Fast CBOR encode/decode (BPv7) requires SSE and SSE2.
* Some optimized loads and stores for TCPCLv4 requires SSE and SSE2.
* Fast CRC32C (BPv7 and a storage hash function) requires SSE4.2.
* The HDTN storage controller will use BITTEST if available.  If BITTEST is not available, it will use ANDN if BMI1 is available.

If `LTP_RNG_USE_RDSEED` is turned on, this feature will be enabled if CMake finds support for this CPU instruction:
* An additional source of randomness for LTP's random number generator requires RDSEED.  You may choose to disable this feature for potentially faster LTP performance even if the CPU supports it.

## Storage Capacity Compilation Parameters ## 
HDTN build environment sets by default two CMake cache variables: STORAGE_SEGMENT_ID_SIZE_BITS and STORAGE_SEGMENT_SIZE_MULTIPLE_OF_4KB.
* The flag `STORAGE_SEGMENT_ID_SIZE_BITS` must be set to 32 (default and recommended) or 64.  It determines the size/type of the storage module's `segment_id_t` Using 32-bit for this type will significantly decrease memory usage (by about half).
    * If this value is 32, The formula for the max segments (S) is given by `S = min(UINT32_MAX, 64^6) = ~4.3 Billion segments` since segment_id_t is a uint32_t. A segment allocator using 4.3 Billion segments uses about 533 MByte RAM), and multiplying by the minimum 4KB block size gives ~17TB bundle storage capacity.  Make sure to appropriately set the `totalStorageCapacityBytes` variable in the HDTN json config so that only the required amount of memory is used for the segment allocator.
    * If this value is 64, The formula for the max segments (S) is given by `S = min(UINT64_MAX, 64^6) = ~68.7 Billion segments` since segment_id_t is a uint64_t. Using a segment allocator with 68.7 Billion segments, when multiplying by the minimum 4KB block size gives ~281TB bundle storage capacity.
* The flag `STORAGE_SEGMENT_SIZE_MULTIPLE_OF_4KB` must be set to an integer of at least 1 (default is 1 and recommended).  It determines the minimum increment of bundle storage based on the standard block size of 4096 bytes.  For example:
    * If `STORAGE_SEGMENT_SIZE_MULTIPLE_OF_4KB` = 1, then a `4KB * 1 = 4KB` block size is used.  A bundle size of 1KB would require 4KB of storage.  A bundle size of 6KB would require 8KB of storage.
    * If `STORAGE_SEGMENT_SIZE_MULTIPLE_OF_4KB` = 2, then a `4KB * 2 = 8KB` block size is used.  A bundle size of 1KB would require 8KB of storage.  A bundle size of 6KB would require 8KB of storage.  A bundle size of 9KB would require 16KB of storage.  If `STORAGE_SEGMENT_ID_SIZE_BITS=32`, then bundle storage capacity could potentially be doubled from ~17TB to ~34TB.

For more information on how the storage works, see `module/storage/doc/storage.pptx` in this repository.

## Logging Compilation Parameters ##
Logging is controlled by CMake cache variables:
* `LOG_LEVEL_TYPE` controls which messages are logged. The options, from most verbose to least verbose, are `TRACE`, `DEBUG`, `INFO`, `WARNING`, `ERROR`, `FATAL`, and `NONE`. All log statements using a level more verbose than the provided level will be compiled out of the application. The default value is `INFO`.
* `LOG_TO_CONSOLE` controls whether log messages are sent to the console. The default value is `ON`.
* `LOG_TO_ERROR_FILE` controls whether all error messages are written to a single error.log file. The default value is `OFF`.
* `LOG_TO_PROCESS_FILE` controls whether each process writes to their own log file. The default value is `OFF`.
* `LOG_TO_SUBPROCESS_FILE` controls whether each subprocess writes to their own log file. The default value is `OFF`.

Getting Started
===============

Build HDTN
===========
If building on Windows, see the [Windows Build Instructions](building_on_windows/readme_building_on_windows.md).
To build HDTN in Release mode (which is now the default if -DCMAKE_BUILD_TYPE is not specified), follow these steps:
* export HDTN_SOURCE_ROOT=/home/username/HDTN
* cd $HDTN_SOURCE_ROOT
* mkdir build
* cd build
* cmake ..
* make -j8
* make install

Note: By Default, BUILD_SHARED_LIBS is OFF and hdtn is built as static
To use shared libs, edit CMakeCache.txt, set BUILD_SHARED_LIBS:BOOL=ON and add fPIC to the Cmakecache variable:
CMAKE_CXX_FLAGS_RELEASE:STRING=-03 -DNDEBUG -fPIC

#### ARM Platforms
HDTN has been tested on various ARM platforms such as Raspberry Pi, Nvidia Jetson Nano and an Ampere Altra Q64-30 based server. To build HDTN in a native ARM environment add the `-DCMAKE_SYSTEM_PROCESSOR` flag to the cmake command. This flag removes x86 optimizations and the x86 unit test. Shared libraries are disabled for ARM builds by default.

- cmake .. -DCMAKE_SYSTEM_PROCESSOR=arm

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

Routing
=======
The Router module runs by default Dijkstra's algorithm on the contact plan to get the next hop for the optimal route leading to the final destination,
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
This repository comes equiped with code to launch a web-based user interface to display statistics for the HDTN engine.
It relies on a dependency called Boost Beast which is packaged as a header-only library that comes with a standard Boost installation.
The web interface will use OpenSSL (if found by CMake) since the web interface supports both http as well as https, and hence both ws (WebSocket) and wss (WebSocket Secure).  If OpenSSL is not found, the web interface will only support http/ws.  The web user interface is enabled by default at compile time.  If the web user interface is not desired, it can be turned off by setting the CMakeCache.txt variable `USE_WEB_INTERFACE:BOOL` to `OFF`.


Now anytime that HDTNOneProcess runs, the web page will be accessible at `http://localhost:8086` and an alternative "system view" gui based on D3.js will be accessible at `http://localhost:8086/hdtn_d3_gui/`

Simulations
=========
HDTN can be simulated using DtnSim, a simulator for delay tolerant networks built on the OMNeT++ simulation framework. Use the "support-hdtn" branch of DtnSim which can be found in the [official DtnSim repository](https://bitbucket.org/lcd-unc-ar/dtnsim/src/support-hdtn/). Currently HDTN simulation with DtnSim has only been tested on Linux (Debian and Ubuntu). Follow the readme instructions for HDTN and DtnSim to install the software. Alternatively, a pre-configured Ubuntu VM is available for download [here](about:blankHDTN%20can%20be%20simulated%20using%20DtnSim,%20a%20simulator%20for%20delay%20tolerant%20networks%20built%20on%20the%20OMNeT++%20simulation%20framework.%20Use%20the%20%22support-hdtn%22%20branch%20of%20DtnSim%20which%20can%20be%20found%20in%20the%20official%20DtnSim%20repository:%20https://bitbucket.org/lcd-unc-ar/dtnsim/src/support-hdtn/.%20Currently%20HDTN%20simulation%20with%20DtnSim%20has%20only%20been%20tested%20on%20Linux%20(Debian%20and%20Ubuntu).%20Follow%20the%20readme%20instructions%20for%20HDTN%20and%20DtnSim%20to%20install%20the%20software.%20Alternatively,%20a%20pre-configured%20Ubuntu%20VM%20is%20available%20here:%20https://drive.google.com/file/d/1dSjxKIZ03U-gsAnMMzcizHAw_gFkDZDe/view?usp=sharing) (the username is hdtnsim-user and password is grc).
More Details about the installation steps can be found [here](https://docs.google.com/document/d/1KrKyO_pr-v9CeS5n_ectpfWtwkYL40PdLICGh-24zZY/edit#)

Docker Instructions
===================
First make sure docker is installed.
```
apt-get install docker
```
Check the service is running 
```
systemctl start docker
```
There are currently two Dockerfiles for building HDTN, one for building an Oracle Linux container and the other for building an Ubuntu. This command will build the Ubuntu one:
```
docker build  -t hdtn_ubuntu containers/docker/ubuntu/.
```
The -t sets the name of the image, in this case hdtn_ubuntu.
Check the image was built with the command:
```
docker images
```
Now to run the container use the command:
```
docker run -d -t hdtn_ubuntu
```
Check that it is running with:
```
docker ps
```
To access it, you'll need the CONTAINER_ID listed with the ps command
```
docker exec -it container_id bash
```
Stop the container with
```
docker stop container_id
```
The same container can either be restarted or removed. To see all the containers Use: 
```
docker ps -a
```
These can still be restarted with the run command above. To remove one that will no longer be used:
```
docker rm container_id
```

Docker Compose Instructions
===========================
Docker compose can be used to spin-up and configure multiple nodes at the same time.
This is done using the docker compose file found under HDTN/containers/docker/docker_compose
```
cd containers/docker/docker_compose
```
This file contains instructions to spin up two containers using Oracle Linux. One is called hdtn_sender and the other hdtn_receiver. Start them with the following command:
```
docker compose up
```
On another bash terminal these can be accessed using the command:
```
docker exec -it hdtn_sender bash
docker exec -it hdtn_receiver bash
```
This setup is perfect for running a test between two hdtn nodes. An example script for each node can be found under HDTN/tests/test_scripts_linux/LTP_2Nodes_Test/. Be sure to run the receiver script first, otherwise the sender will have nowhere to send to at launch.

Kubernetes Instructions
=======================
Download the dependencies
```
sudo apt-install docker microk8s
```

The first step is to create a docker images to be pushed locally for kubernetes to pull:
```
docker build docker/ubuntu/. -t myhdtn:local
```
Check that it was built:
```
docker images
```
Next we build the image locally and inject it into the microk8s image cache
```
docker save myhdtn > myhdtn.tar
microk8s ctr image import myhdtn.tar
```
Confirm this with:
```
microk8s ctr images ls
```
Now we deploy the cluster, the yaml must reference the injected image name
```
microk8s kubectl apply -f containers/kubernetes/hdtn_10_node_cluster.yaml 
```
There should now be ten kubernetes pods running with HDTN. See them with:
```
microk8s kubectl get pods
```
To access a container in a pod, enter the following command:
```
microk8s kubectl exec -it container_name -- bash
```
When you're finished working with this deployment, delete it using:
```
microk8s kubectl delete deployment hdtn-deployment
```
Use the get pods command to confirm they've been deleted
```
microk8s kubectl get pods
```
