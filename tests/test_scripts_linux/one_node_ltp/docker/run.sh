#!/bin/sh

# Build this repo as a Docker image
docker build $HDTN_STREAMING_DIR -t hdtn-streaming

# Run the test
docker run -u root --rm --network host --name hdtn-streaming --entrypoint "tests/test_scripts_linux/one_node_ltp/docker/entrypoint.sh" hdtn-streaming:latest &

# Wait for stream to start
sleep 5

# Launch VLC, if installed
if dpkg -s vlc &> /dev/null; then
    vlc $HDTN_STREAMING_DIR/tests/test_scripts_linux/one_node_ltp/hdtn-stream.sdp
else
    echo "To view the stream, open hdtn-stream.sdp with VLC"
    sleep 1000
fi

# Remove docker container
docker rm -f hdtn-streaming