#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_CACHE_DIR="${SCRIPT_DIR}/cache"

# Read and validate required variables
REQUIRED_VARS=("IDF_CLONE_BRANCH_OR_TAG" "IDF_CHECKOUT_REF" "IDF_CLONE_SHALLOW" "IDF_CLONE_SHALLOW_DEPTH" "BASE_IMAGE_NAME")
source "${SCRIPT_DIR}/.env"
for var in "${REQUIRED_VARS[@]}"; do
  if [ -z "${!var}" ]; then
    echo "❌ Missing required environment variable: $var. Please check .env file."
    exit 1
  fi
done

# Ensure cache directory exists
if [ ! -d "$IMAGE_CACHE_DIR" ]; then
  mkdir -p "$IMAGE_CACHE_DIR"
  echo "➕ Cache directory created: $IMAGE_CACHE_DIR"
else
  echo "📦 Using cache directory: $IMAGE_CACHE_DIR"
fi

# Fetch base Dockerfile from Espressif using selected commit, skip if cached
IDF_BASE_IMAGE_URL="https://raw.githubusercontent.com/espressif/esp-idf/${IDF_CHECKOUT_REF}/tools/docker"
if [ ! -f "$IMAGE_CACHE_DIR/Dockerfile" ]; then
  echo "⬇️  Fetching base ESP-IDF Dockerfile from espressif repo on branch \"$IDF_CLONE_BRANCH_OR_TAG\"..."
  curl -fsSL "$IDF_BASE_IMAGE_URL/Dockerfile" -o "$IMAGE_CACHE_DIR/Dockerfile"
  curl -fsSL "$IDF_BASE_IMAGE_URL/entrypoint.sh" -o "$IMAGE_CACHE_DIR/entrypoint.sh"
else
  echo "📦 Using cached base ESP-IDF Dockerfile. Skipping download."
fi
echo "🔧 Base image arguments:"
echo -e "\t -- IDF_CLONE_BRANCH_OR_TAG = $IDF_CLONE_BRANCH_OR_TAG"
echo -e "\t -- IDF_CHECKOUT_REF = $IDF_CHECKOUT_REF"
echo -e "\t -- IDF_CLONE_SHALLOW = $IDF_CLONE_SHALLOW"
echo -e "\t -- IDF_CLONE_SHALLOW_DEPTH = $IDF_CLONE_SHALLOW_DEPTH"
echo -e "\t -- IDF_INSTALL_TARGETS = $IDF_INSTALL_TARGETS"
echo -e "\t -- BASE_IMAGE_NAME = $BASE_IMAGE_NAME"

# Check if the base image exists locally or on Docker Hub, otherwise build it
echo "🔍 Checking for local image '$BASE_IMAGE_NAME'..."
if docker image inspect "$BASE_IMAGE_NAME" > /dev/null 2>&1; then
    echo "✅ Image exists locally, skipping."
    exit 0
fi
echo "🌐 Checking Docker Hub for image '$BASE_IMAGE_NAME'..."
if docker manifest inspect "$BASE_IMAGE_NAME" > /dev/null 2>&1; then
    echo "⬇️  Pulling image from Docker Hub..."
    docker pull "$BASE_IMAGE_NAME"
    exit 0
fi
echo "⚒️  Image not found locally or on Docker Hub. Building base IDF image: $BASE_IMAGE_NAME"
docker build -t "$BASE_IMAGE_NAME" \
  --build-arg IDF_CLONE_BRANCH_OR_TAG="$IDF_CLONE_BRANCH_OR_TAG" \
  --build-arg IDF_CHECKOUT_REF="$IDF_CHECKOUT_REF" \
  --build-arg IDF_CLONE_SHALLOW="$IDF_CLONE_SHALLOW" \
  --build-arg IDF_CLONE_SHALLOW_DEPTH="$IDF_CLONE_SHALLOW_DEPTH" \
  --build-arg IDF_INSTALL_TARGETS="$IDF_INSTALL_TARGETS" \
  -f "$IMAGE_CACHE_DIR/Dockerfile" $IMAGE_CACHE_DIR
echo "✅ Build complete: $BASE_IMAGE_NAME"