# HDTN Media Streaming

## Introduction

This repository contains media streaming plugin applications for the High-Rate Delay Tolerant Network (HDTN) software. 

Once built, the `bpsend_stream` executable can be used to ingest media such as RTP packets and mp4 files, and send them to a Delay Tolerant Network (DTN) as bundles over any supported convergence layer. Similarly, the `bprecv_stream` executable can be used to receive bundles from a DTN and reassemble the media.

## Installation using Docker

These applications depend on HDTN and Gstreamer libraries. So far, they have only been tested on Debian-based Linux distributions. The easiest way to get started is by building `Dockerfile` in the root directory:

```
docker build -t hdtn-streaming .
```

And then running a container with the host network attached, like:
```
docker run -it -u root --network host --name hdtn-streaming hdtn-streaming:latest /bin/bash
```

However, if you would like to run `bpsend_stream` and `bprecv_stream` natively, you can follow the instructions below.

## Installation on native machine and Build Dependencies

##### HDTN

1. Build and install HDTN using the instructions located [in GitHub](https://gitlab.grc.nasa.gov/hdtn-v4/hdtn)

```
git clone https://github.com/nasa/HDTN
cd HDTN && git checkout development && mkdir build && cd build && cmake .. && make -j6 && sudo make install
```

##### Gstreamer

1. Install OS dependencies
```
sudo apt-get update -y && sudo apt-get install -y pkg-config \
    flex bison git python3-pip ninja-build libx264-dev \
    cmake build-essential libzmq3-dev libboost-dev libboost-all-dev openssl libssl-dev
```
2. Install meson and the directory where it was installed to PATH 
```
pip3 install meson
```
3. Clone, build, and install Gstreamer
```
git clone https://github.com/GStreamer/gstreamer.git \
cd gstreamer && meson setup build \
meson configure build && meson compile -C build \
ninja -C build install 
```

#### Build Media Streaming

1. Clone this repository, and build using CMake

```
git clone TDB
cd HDTN-Streaming && mkdir build && cd build && \
    cmake .. && make
```

## Example usage with Docker

Tests scripts are located at `tests/test_scripts_linux`. After cloning this repository, and before running any test, set `HDTN_STREAMING_DIR` to
the root of the repository, e.g.:
```
export HDTN_STREAMING_DIR=/home/user/HDTN-Streaming
```

To quickly get started using Docker:
1. Install Docker (https://docs.docker.com/engine/install/ubuntu/)
2. Download VLC
```
apt-get install vlc
```
3. Run the `one_node_ltp` test, to stream a video file using `bpsend_stream`, `bprecv_stream`, and HDTN:
```
./tests/test_scripts_linux/one_node_ltp/docker/run.sh
```
