FROM ubuntu:latest
#gs490v-gitlab.ndc.nasa.gov:80/493_lcrd/docker/ubi/ubi8:latest

LABEL Maintainer="Blake LaFuente, NASA GRC"
LABEL Description="HDTN built using Ubuntu"

RUN apt-get update && apt-get -y install cmake build-essential libzmq3-dev libboost-dev libboost-all-dev openssl libssl-dev git

RUN git clone https://github.com/nasa/HDTN.git
ARG SW_DIR=/HDTN
ENV HDTN_SOURCE_ROOT=${SW_DIR}
RUN cd $HDTN_SOURCE_ROOT && mkdir build && cd build && cmake .. && make


#may have to remove '-j8' (number of cores used to compile)

#Send instructions


