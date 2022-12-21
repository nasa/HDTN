FROM oraclelinux:8
#gs490v-gitlab.ndc.nasa.gov:80/493_lcrd/docker/ubi/ubi8:latest

LABEL Maintainer="Blake LaFuente, NASA GRC"
LABEL Description="HDTN built using RHEL"


RUN yum update -y && dnf -y update && dnf install -y epel-release && yum install -y gcc gcc-c++ cmake boost-devel openssl-devel zeromq zeromq-devel git

RUN git clone https://github.com/nasa/HDTN.git 
ARG SW_DIR=/HDTN
ENV HDTN_SOURCE_ROOT=${SW_DIR} 
RUN cd $HDTN_SOURCE_ROOT && mkdir build && cd build && cmake .. && make

#can speed up build by changing last command to 'make -j8' (number of cores used to compile)




