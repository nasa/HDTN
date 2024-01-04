# Minimal Debian-based HDTN docker container

To build:

```
cd $HDTN_SOURCE_ROOT && \
docker build \
    --tag hdtn:minimal \
    --file ./containers/docker/debian-minimal/Dockerfile .
```
