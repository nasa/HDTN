FROM oraclelinux:8

LABEL Maintainer="Blake LaFuente, NASA GRC"
LABEL Description="HDTN built using RHEL"


RUN yum update -y && dnf -y update && dnf install -y epel-release && yum install -y gcc gcc-c++ cmake boost-devel openssl-devel zeromq zeromq-devel git sysvinit-tools


RUN git clone https://github.com/nasa/HDTN.git 
ARG SW_DIR=/HDTN
ENV HDTN_SOURCE_ROOT=${SW_DIR} 
RUN cd $HDTN_SOURCE_ROOT && mkdir build && cd build && cmake .. && make -j8

#remove -j8 if fewer than 8 cores

