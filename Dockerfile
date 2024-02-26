FROM ubuntu:mantic

ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC
ENV HDTN_STREAMING_DIR=.
ENV GST_PATH=/gstreamer

# Install OS dependencies
RUN apt-get update -y && apt-get install -y pkg-config \
    flex bison git python3-pip ninja-build libx264-dev \
    cmake build-essential libzmq3-dev libboost-dev libboost-all-dev openssl libssl-dev \
    wget

# Install Python packages
RUN pip3 install --break-system-packages  meson

# Build and install HDTN
RUN git clone https://github.com/nasa/HDTN
RUN cd HDTN && git checkout development && mkdir build && cd build && cmake .. && make -j6 && make install

# Build and install Gstreamer
RUN git clone https://github.com/GStreamer/gstreamer.git
RUN cd gstreamer && meson setup build
RUN meson configure gstreamer/build && meson compile -C gstreamer/build
RUN ninja -C gstreamer/build install

# Copy and build streaming applications
COPY . .
RUN mkdir -p build && cd build && cmake .. && make -j6
