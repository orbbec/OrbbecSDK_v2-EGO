# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

# Build only the static library; HTTP only, no TLS, no optional third-party libs.
# Using HTTP_ONLY disables all non-HTTP protocols in one shot.
set(BUILD_CURL_EXE          OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS       OFF CACHE BOOL "" FORCE)
set(HTTP_ONLY               ON  CACHE BOOL "" FORCE)

# Disable all TLS backends — the EGO file-service runs on plain http://.
# Platform-native backends are also explicitly disabled to prevent auto-detection.
set(CURL_USE_OPENSSL        OFF CACHE BOOL "" FORCE)
set(CURL_USE_MBEDTLS        OFF CACHE BOOL "" FORCE)
set(CURL_USE_WOLFSSL        OFF CACHE BOOL "" FORCE)
set(CURL_USE_SECTRANSP      OFF CACHE BOOL "" FORCE)  # macOS / iOS SecureTransport
set(CURL_USE_SCHANNEL       OFF CACHE BOOL "" FORCE)  # Windows Schannel
set(CURL_WINDOWS_SSPI       OFF CACHE BOOL "" FORCE)  # Windows SSPI auth
set(CURL_ENABLE_SSL         OFF CACHE BOOL "" FORCE)  # master SSL switch

# Disable all optional third-party libraries to prevent auto-detection
# on CI machines (macOS/Linux) that may have these installed.
set(CURL_USE_LIBSSH2        OFF CACHE BOOL "" FORCE)
set(USE_LIBSSH2             OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH         OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBIDN2        OFF CACHE BOOL "" FORCE)
set(USE_LIBIDN2             OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL         OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBGSASL       OFF CACHE BOOL "" FORCE)
set(CURL_USE_GSSAPI         OFF CACHE BOOL "" FORCE)
set(CURL_BROTLI             OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD               OFF CACHE BOOL "" FORCE)
set(CURL_ZLIB               OFF CACHE BOOL "" FORCE)
set(ENABLE_ARES             OFF CACHE BOOL "" FORCE)

# Disable unused curl features
set(CURL_DISABLE_ALTSVC     ON  CACHE BOOL "" FORCE)
set(CURL_DISABLE_HSTS       ON  CACHE BOOL "" FORCE)
set(CURL_DISABLE_COOKIES    ON  CACHE BOOL "" FORCE)
set(CURL_DISABLE_CRYPTO_AUTH ON  CACHE BOOL "" FORCE)
set(ENABLE_MANUAL           OFF CACHE BOOL "" FORCE)
set(ENABLE_CURL_MANUAL      OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING           OFF CACHE BOOL "" FORCE)
set(USE_NGHTTP2             OFF CACHE BOOL "" FORCE)
set(USE_NGTCP2              OFF CACHE BOOL "" FORCE)
set(USE_QUICHE              OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES          OFF CACHE BOOL "" FORCE)

add_subdirectory(src)

set_target_properties(libcurl_static PROPERTIES FOLDER "dependencies")

if(TARGET libcurl_object)
    set_target_properties(libcurl_object PROPERTIES FOLDER "dependencies")
endif()

get_target_property(_curl_opts libcurl_static COMPILE_OPTIONS)
if(_curl_opts)
    list(REMOVE_ITEM _curl_opts "-Werror" "/WX" "-Wpedantic")
    set_target_properties(libcurl_static PROPERTIES COMPILE_OPTIONS "${_curl_opts}")
endif()
