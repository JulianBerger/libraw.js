#!/bin/bash

# Extract version from package.json
CURRENT_LIBRAWJS_VERSION=$(node -p "require('./package.json').version")

# Function to build for a specific architecture
build_arch() {
    local arch=$1
    local platform="linux"
    local docker_platform="linux/$arch"

    # Map Node.js architecture names to Docker platform names
    if [ "$arch" == "x64" ]; then
        docker_platform="linux/amd64"
    fi

    PREBUILD_PATH="prebuilds/${platform}-${arch}"
    NODE_FILE_NAME="@julianberger+libraw.js.node"
    OUTPUT_PATH="./${PREBUILD_PATH}/${NODE_FILE_NAME}"
    BACKUP_PATH="./${PREBUILD_PATH}/bak.${NODE_FILE_NAME}"
    IMAGE_TAG_NAME="libraw.js:${CURRENT_LIBRAWJS_VERSION}-${arch}"

    echo "Building for ${platform}-${arch} (version ${CURRENT_LIBRAWJS_VERSION})"
    echo "Output path is ${OUTPUT_PATH}"

    if [ ! -d ${PREBUILD_PATH} ]; then
        echo "${platform}-${arch} prebuild dir does not exist, creating."
        mkdir -p ${PREBUILD_PATH}
    else
        echo "${platform}-${arch} prebuild dir exists."
    fi

    if [ -f ${OUTPUT_PATH} ]; then
        echo "${platform}-${arch} prebuild file exists. Backing up."
        mv ${OUTPUT_PATH} ${BACKUP_PATH}
    fi

    DOCKER_BUILDKIT=1
    docker build -t ${IMAGE_TAG_NAME} --output ${PREBUILD_PATH} --no-cache --platform ${docker_platform} .

    DOCKER_EXIT_CODE=$?

    if [ -f ${BACKUP_PATH} ] && [ $DOCKER_EXIT_CODE -eq 0 ]; then
        echo "Binary build succeeded. Deleting backup file."
        rm ${BACKUP_PATH}
    fi

    if [ $DOCKER_EXIT_CODE -eq 0 ]; then
        echo "${platform}-${arch} build complete."
    else
        echo "Docker build failed for ${platform}-${arch}."
        exit 1
    fi
}

# Check if architecture argument is provided
if [ "$1" == "" ]; then
    echo "Building for both x64 and ARM64..."
    build_arch "x64"
    build_arch "arm64"
    echo "All Linux builds completed!"
elif [ "$1" == "x64" ] || [ "$1" == "arm64" ]; then
    build_arch "$1"
else
    echo "Usage: $0 [x64|arm64]"
    echo "If no argument is provided, both architectures will be built."
    exit 1
fi