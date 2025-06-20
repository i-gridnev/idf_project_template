#!/bin/sh
IMAGE_NAME=$1
if docker image inspect $IMAGE_NAME >/dev/null 2>&1; then
    echo "IDF image exists locally: $IMAGE_NAME"
else
    echo "Building development IDF image $IMAGE_NAME..."
    docker build -t $IMAGE_NAME ./.devcontainer
fi