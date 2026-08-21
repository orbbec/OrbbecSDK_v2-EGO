// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
//
// EgoPro RockUSB upgrade - C ABI declaration.
#ifndef EGOPRO_UPGRADE_API_H
#define EGOPRO_UPGRADE_API_H

#include <stddef.h>

#if defined(_WIN32)
#define EGOPRO_API __declspec(dllexport)
#else
#define EGOPRO_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*egopro_progress_cb)(int percent, const char *message, void *user);

EGOPRO_API int egopro_rk_upgrade_file(const char *image_path, egopro_progress_cb cb, void *user, const volatile int *cancel_flag);

#ifdef __cplusplus
}
#endif

#endif  // EGOPRO_UPGRADE_API_H
