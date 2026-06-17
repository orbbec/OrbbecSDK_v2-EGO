#!/usr/bin/env bash

# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

set -euo pipefail

CURRENT_DIR=$(pwd)

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/../../" && pwd)

# current os
SYSTEM=$(uname -s)
if [ "${SYSTEM}" != "Darwin" ]; then
    echo "Invalid os. The current os should be macOS"
    exit 1
fi

DEFAULT_LIB_NAME="OrbbecSDK"
SDK_LIB_NAME=${DEFAULT_LIB_NAME}
BUILD_TYPE="Release"
IOS_PLATFORM="OS64"
ENABLE_BITCODE="FALSE"

VERSION=$(grep -o "project(.* VERSION [0-9]*\.[0-9]*\.[0-9]*" "${PROJECT_ROOT}/CMakeLists.txt" | grep -o "[0-9]*\.[0-9]*\.[0-9]*")
TIMESTAMP=$(date +"%Y%m%d%H%M")
COMMIT_HASH=$(git -C "${PROJECT_ROOT}" rev-parse --short HEAD)
PACKAGE_NAME="${SDK_LIB_NAME}_v${VERSION}_${TIMESTAMP}_${COMMIT_HASH}_iOS_${IOS_PLATFORM}"
BUILD_DIR="${PROJECT_ROOT}/build_iOS_${IOS_PLATFORM}"
INSTALL_DIR="${BUILD_DIR}/install/${PACKAGE_NAME}"
TOOLCHAIN_FILE="${PROJECT_ROOT}/cmake/ios.toolchain.cmake"

echo "Building ${SDK_LIB_NAME} for iOS"
echo ">>>>> iOS platform: ${IOS_PLATFORM}"
echo ">>>>> build config: ${BUILD_TYPE}"
echo ">>>>> SDK library name: ${SDK_LIB_NAME}"
echo ">>>>> enable bitcode: ${ENABLE_BITCODE}"
echo ""

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
mkdir -p "${INSTALL_DIR}"

cmake_command=(
    cmake
    -S "${PROJECT_ROOT}"
    -B "${BUILD_DIR}"
    -G "Unix Makefiles"
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}"
    -DPLATFORM="${IOS_PLATFORM}"
    -DENABLE_BITCODE="${ENABLE_BITCODE}"
    -DBUILD_SHARED_LIBS=ON
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}"
    -DOPEN_SDK_LIB_NAME="${SDK_LIB_NAME}"
    -DOB_BUILD_EXAMPLES=OFF
    -DOB_BUILD_TOOLS=OFF
    -DOB_BUILD_DOCS=OFF
    -DOB_BUILD_TESTS=OFF
    -DOB_INSTALL_EXAMPLES_SOURCE=OFF
    -DOB_BUILD_USB_PAL=OFF
    -DOB_BUILD_NET_PAL=ON
    -DOB_BUILD_GMSL_PAL=OFF
)

echo "cmake command = ${cmake_command[*]}"
echo ">> Configuring iOS build"
"${cmake_command[@]}"

echo ">> Building and installing iOS SDK"
cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" --target install

if [ "${SDK_LIB_NAME}" != "${DEFAULT_LIB_NAME}" ] && [ -d "${INSTALL_DIR}" ]; then
    find "${INSTALL_DIR}" -type f \( -name "*.sh" -o -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.txt" \) | while read -r file; do
        sed -i '' '/Copyright/d;/Licensed/d' "${file}"
    done
fi

cd "${BUILD_DIR}/install"
tar -czf "${PACKAGE_NAME}.tar.gz" "${PACKAGE_NAME}"

echo "Done building and compressing ${SDK_LIB_NAME} for iOS ${IOS_PLATFORM}"

cd "${CURRENT_DIR}"