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
SDK_LIB_NAME=${OPEN_SDK_LIB_NAME:-${DEFAULT_LIB_NAME}}
BUILD_TYPE=${BUILD_TYPE:-Release}
IOS_PLATFORM="OS64"
ENABLE_BITCODE=${ENABLE_BITCODE:-FALSE}
BUILD_JOBS=${BUILD_JOBS:-$(sysctl -n hw.ncpu)}
IOS_CODESIGN=${IOS_CODESIGN:-ON}
IOS_CODESIGN_IDENTITY=${IOS_CODESIGN_IDENTITY:--}

VERSION=$(grep -o "project(.* VERSION [0-9]*\.[0-9]*\.[0-9]*" "${PROJECT_ROOT}/CMakeLists.txt" | grep -o "[0-9]*\.[0-9]*\.[0-9]*")
TIMESTAMP=$(date +"%Y%m%d%H%M")
COMMIT_HASH=$(git -C "${PROJECT_ROOT}" rev-parse --short HEAD)
PACKAGE_NAME="${SDK_LIB_NAME}_v${VERSION}_${TIMESTAMP}_${COMMIT_HASH}_iOS_${IOS_PLATFORM}"
PACKAGE_ROOT="${PROJECT_ROOT}/build_iOS_${IOS_PLATFORM}"
BUILD_DIR="${PACKAGE_ROOT}/build"
INSTALL_DIR="${PACKAGE_ROOT}/install/${PACKAGE_NAME}"
ARTIFACTS_DIR="${PACKAGE_ROOT}/artifacts/${PACKAGE_NAME}"
TOOLCHAIN_FILE="${PROJECT_ROOT}/cmake/ios.toolchain.cmake"
FRAMEWORK_NAME="${SDK_LIB_NAME}"

echo "Building ${SDK_LIB_NAME} for iOS"
echo ">>>>> iOS platform: ${IOS_PLATFORM}"
echo ">>>>> build config: ${BUILD_TYPE}"
echo ">>>>> SDK library name: ${SDK_LIB_NAME}"
echo ">>>>> enable bitcode: ${ENABLE_BITCODE}"
echo ">>>>> parallel jobs: ${BUILD_JOBS}"
echo ">>>>> iOS codesign: ${IOS_CODESIGN}"
echo ">>>>> iOS codesign identity: ${IOS_CODESIGN_IDENTITY}"
echo ""

rm -rf "${PACKAGE_ROOT}"
mkdir -p "${BUILD_DIR}" "${INSTALL_DIR}" "${ARTIFACTS_DIR}"

cmake_command=(
    cmake
    -S "${PROJECT_ROOT}"
    -B "${BUILD_DIR}"
    -G "Unix Makefiles"
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}"
    -DPLATFORM="${IOS_PLATFORM}"
    -DENABLE_BITCODE="${ENABLE_BITCODE}"
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}"
    -DOPEN_SDK_LIB_NAME="${SDK_LIB_NAME}"
    -DOB_BUILD_EXAMPLES=OFF
    -DOB_BUILD_TOOLS=OFF
    -DOB_BUILD_DOCS=OFF
    -DOB_BUILD_TESTS=OFF
    -DOB_INSTALL_FILTER_DEV_HEADERS=ON
    -DOB_INSTALL_EXAMPLES_SOURCE=OFF
    -DOB_BUILD_USB_PAL=OFF
    -DOB_BUILD_NET_PAL=ON
    -DOB_BUILD_GMSL_PAL=OFF
    -DOB_BUILD_SOVERSION=OFF
)

echo "cmake command = ${cmake_command[*]}"
echo ">> Configuring iOS build"
"${cmake_command[@]}"

echo ">> Building and installing iOS SDK"
cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" --target install --parallel "${BUILD_JOBS}"

locate_dylib() {
    local preferred="${INSTALL_DIR}/lib/lib${SDK_LIB_NAME}.dylib"

    if [ -f "${preferred}" ]; then
        echo "${preferred}"
        return 0
    fi

    local first_match
    first_match=$(find "${INSTALL_DIR}/lib" -maxdepth 1 -type f -name "lib${SDK_LIB_NAME}*.dylib" | sort | head -n 1)
    if [ -z "${first_match}" ]; then
        echo "Failed to locate installed dylib under ${INSTALL_DIR}/lib" >&2
        exit 1
    fi

    echo "${first_match}"
}

locate_config_xml() {
    local preferred="${INSTALL_DIR}/lib/${SDK_LIB_NAME}Config.xml"

    if [ -f "${preferred}" ]; then
        echo "${preferred}"
        return 0
    fi

    local fallback="${INSTALL_DIR}/shared/${SDK_LIB_NAME}Config.xml"
    if [ -f "${fallback}" ]; then
        echo "${fallback}"
        return 0
    fi

    echo ""
}

create_framework_bundle() {
    local binary_source=$1
    local framework_dir=$2
    local headers_dir="${framework_dir}/Headers"
    local modules_dir="${framework_dir}/Modules"
    local plist_path="${framework_dir}/Info.plist"
    local framework_binary="${framework_dir}/${FRAMEWORK_NAME}"
    local config_xml

    rm -rf "${framework_dir}"
    mkdir -p "${headers_dir}" "${modules_dir}"

    cp -R "${INSTALL_DIR}/include/." "${headers_dir}/"
    cp "${binary_source}" "${framework_binary}"

    config_xml=$(locate_config_xml)
    if [ -n "${config_xml}" ]; then
        cp "${config_xml}" "${framework_dir}/${SDK_LIB_NAME}Config.xml"
    fi

    cat > "${modules_dir}/module.modulemap" <<EOF
framework module ${FRAMEWORK_NAME} {
  umbrella "Headers"
  export *
  module * { export * }
}
EOF

    cat > "${plist_path}" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>${FRAMEWORK_NAME}</string>
    <key>CFBundleIdentifier</key>
    <string>com.orbbec.${FRAMEWORK_NAME}</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>${FRAMEWORK_NAME}</string>
    <key>CFBundlePackageType</key>
    <string>FMWK</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>MinimumOSVersion</key>
    <string>13.0</string>
</dict>
</plist>
EOF

    install_name_tool -id "@rpath/${FRAMEWORK_NAME}.framework/${FRAMEWORK_NAME}" "${framework_binary}"
}

is_codesign_enabled() {
    case "$(echo "$IOS_CODESIGN" | tr '[:upper:]' '[:lower:]')" in
        on|true|1|yes)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

sign_path() {
    local path="$1"
    if ! is_codesign_enabled; then
        return 0
    fi

    if ! command -v codesign >/dev/null 2>&1; then
        echo "codesign tool not found" >&2
        exit 1
    fi

    if [ ! -e "$path" ]; then
        echo "Path not found for codesign: $path" >&2
        exit 1
    fi

    codesign --force --sign "$IOS_CODESIGN_IDENTITY" --timestamp=none "$path"
}

sign_framework_bundle() {
    local framework_dir="$1"

    if ! is_codesign_enabled; then
        return 0
    fi

    sign_path "${framework_dir}/${FRAMEWORK_NAME}"
    sign_path "${framework_dir}"
}

# iOS apps cannot ship bare dylibs nested inside a framework, so every extension library is
# packaged as its own minimal framework (binary + Info.plist) that the app embeds next to the
# SDK framework. The SDK locates them at runtime as sibling frameworks via dlopen.
create_extension_frameworks() {
    local extensions_src="${INSTALL_DIR}/lib/extensions"
    local dest_dir="${ARTIFACTS_DIR}/frameworks/device/extensions"

    if [ ! -d "${extensions_src}" ]; then
        return 0
    fi

    mkdir -p "${dest_dir}"

    local dylib_path
    while IFS= read -r -d '' dylib_path; do
        local dylib_name ext_name framework_dir
        dylib_name=$(basename "${dylib_path}")
        ext_name="${dylib_name%.dylib}"
        ext_name="${ext_name#lib}"
        framework_dir="${dest_dir}/${ext_name}.framework"

        rm -rf "${framework_dir}"
        mkdir -p "${framework_dir}"
        cp "${dylib_path}" "${framework_dir}/${ext_name}"

        cat > "${framework_dir}/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>${ext_name}</string>
    <key>CFBundleIdentifier</key>
    <string>com.orbbec.${ext_name}</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>${ext_name}</string>
    <key>CFBundlePackageType</key>
    <string>FMWK</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>MinimumOSVersion</key>
    <string>13.0</string>
</dict>
</plist>
EOF

        install_name_tool -id "@rpath/${ext_name}.framework/${ext_name}" "${framework_dir}/${ext_name}"

        sign_path "${framework_dir}/${ext_name}"
        sign_path "${framework_dir}"

        echo "Created extension framework: ${ext_name}.framework"
    done < <(find "${extensions_src}" -type f -name "*.dylib" -print0)
}

create_cmake_package_wrapper() {
    local cmake_dir="${ARTIFACTS_DIR}/lib"

    mkdir -p "${cmake_dir}"

    cat > "${cmake_dir}/${SDK_LIB_NAME}Config.cmake" <<EOF
if(TARGET ob::${SDK_LIB_NAME})
    return()
endif()

if(NOT APPLE)
    message(FATAL_ERROR "The packaged iOS ${SDK_LIB_NAME} can only be consumed from Apple toolchains.")
endif()

get_filename_component(_IMPORT_PREFIX "\${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(_IMPORT_PREFIX "\${_IMPORT_PREFIX}" PATH)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_OSX_SYSROOT MATCHES "iphone|iPhone")
    if(CMAKE_OSX_SYSROOT MATCHES "simulator|Simulator")
        message(FATAL_ERROR "The packaged iOS ${SDK_LIB_NAME} only supports iPhoneOS device builds.")
    endif()
    set(_orbbecsdk_framework_dir "\${_IMPORT_PREFIX}/frameworks/device/${FRAMEWORK_NAME}.framework")
else()
    message(FATAL_ERROR "The packaged ios ${SDK_LIB_NAME} should only be selected for iOS builds.")
endif()

if(NOT EXISTS "\${_orbbecsdk_framework_dir}/${FRAMEWORK_NAME}")
    message(FATAL_ERROR "Missing ${FRAMEWORK_NAME} framework binary at \${_orbbecsdk_framework_dir}/${FRAMEWORK_NAME}")
endif()

add_library(ob::${SDK_LIB_NAME} SHARED IMPORTED)
set_target_properties(ob::${SDK_LIB_NAME} PROPERTIES
    IMPORTED_LOCATION "\${_orbbecsdk_framework_dir}/${FRAMEWORK_NAME}"
    IMPORTED_NO_SONAME TRUE
    INTERFACE_INCLUDE_DIRECTORIES "\${_orbbecsdk_framework_dir}/Headers"
)

unset(_orbbecsdk_framework_dir)
unset(_IMPORT_PREFIX)
EOF
}

if [ "${SDK_LIB_NAME}" != "${DEFAULT_LIB_NAME}" ] && [ -d "${INSTALL_DIR}" ]; then
    find "${INSTALL_DIR}" -type f \( -name "*.sh" -o -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.txt" \) | while read -r file; do
        sed -i '' '/Copyright/d;/Licensed/d' "${file}"
    done
fi

DEVICE_DYLIB=$(locate_dylib)
FRAMEWORK_DIR="${ARTIFACTS_DIR}/frameworks/device/${FRAMEWORK_NAME}.framework"
mkdir -p "${ARTIFACTS_DIR}/frameworks/device"

create_framework_bundle "${DEVICE_DYLIB}" "${FRAMEWORK_DIR}"
sign_framework_bundle "${FRAMEWORK_DIR}"
create_extension_frameworks
create_cmake_package_wrapper

cat > "${ARTIFACTS_DIR}/README.txt" <<EOF
${FRAMEWORK_NAME}.framework

Contents:
- frameworks/device/${FRAMEWORK_NAME}.framework: app-facing iPhoneOS device framework
- frameworks/device/extensions/*.framework: private extension frameworks (filters, frame processor, firmware updater)
- lib/${SDK_LIB_NAME}Config.cmake: downstream CMake import wrapper

Notes:
- iOS apps cannot ship bare dylibs nested inside a framework, so every extension library is packaged
  as a standalone framework; embed ${FRAMEWORK_NAME}.framework and all extension frameworks in the app
  target ("Embed & Sign") so they land side by side in the app's Frameworks directory
- The SDK locates extension frameworks at runtime as siblings of ${FRAMEWORK_NAME}.framework via dlopen
- ${SDK_LIB_NAME}Config.xml is bundled at framework root because the SDK resolves it relative to the library location
- Raw CMake install tree is preserved under ${INSTALL_DIR}
- Simulator support is intentionally not packaged; this distribution targets iPhoneOS devices only
EOF

cd "${PACKAGE_ROOT}/artifacts"
tar -czf "${PACKAGE_NAME}.tar.gz" "${PACKAGE_NAME}"

echo "Done building and packaging ${SDK_LIB_NAME} for iOS ${IOS_PLATFORM}"
echo "Artifacts: ${ARTIFACTS_DIR}"

cd "${CURRENT_DIR}"