FROM registry.access.redhat.com/ubi8/ubi:latest
#gs490v-gitlab.ndc.nasa.gov:80/493_lcrd/docker/ubi/ubi8:latest

LABEL Maintainer="Blake LaFuente, NASA GRC"
LABEL Description="HDTN built using RHEL"


RUN yum update -y && dnf -y update && dnf clean all && dnf install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm && yum install -y cmake boost-devel zeromq zeromq-devel git

RUN git clone https://github.com/nasa/HDTN.git 
ARG SW_DIR=/HDTN
ENV HDTN_SOURCE_ROOT=${SW_DIR} 
RUN cd $HDTN_SOURCE_ROOT && mkdir build && cd build && cmake .. && make

#can speed up build by changing last command to 'make -j8' (number of cores used to compile)




