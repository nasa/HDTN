##############################################
# Builder
##############################################

FROM debian:11-slim AS builder
ARG CORES=4

# Build dependencies
RUN apt-get update && \
    apt-get install -y \
        unzip \
        cmake \
        build-essential \
        libboost-dev \
        libboost-all-dev \
        openssl \
        libssl-dev \
        libzmq3-dev && \
    rm -rf /var/lib/apt/lists/*

# Copy in, build, install HDTN
COPY . /opt/hdtn-src
RUN cd /opt/hdtn-src && \
    mkdir -p docker-build && \
    cd docker-build && \
    cmake \
        -DBoost_USE_STATIC_LIBS=ON \
        -DCMAKE_INSTALL_PREFIX=/opt/hdtn \
        .. && \
    make -j $CORES && \
    make install

##############################################
# HDTN Container
##############################################
FROM debian:11-slim

RUN apt-get update && \
    apt-get install -y \
        libc6 \
        libstdc++6 \
        libzmq5 && \
    rm -rf /var/lib/apt/lists/*

COPY --from=builder /opt/hdtn /opt/hdtn
ENV PATH="${PATH}:/opt/hdtn/bin"
