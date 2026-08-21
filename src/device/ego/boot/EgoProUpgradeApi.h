// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
//
// C ABI for the EgoPro RockUSB upgrade. the SDK calls egopro_rk_upgrade_file() directly through this declaration.
#ifndef EGOPRO_UPGRADE_API_H
#define EGOPRO_UPGRADE_API_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Progress notification. percent is in [0,100]; message is a short status
// string owned by the DLL (valid only for the duration of the call).
typedef void (*egopro_progress_cb)(int percent, const char *message, void *user);

// Flash the RockUSB update image located at image_path onto the first EgoPro
// device found in RockUSB download (Loader) mode.
//   cb / user   - optional progress callback and its opaque context.
//   cancel_flag - optional pointer to an int polled by the DLL; when it becomes
//                 non-zero the upgrade aborts as soon as possible.
// Returns 0 on success, a negative error code on failure.
int egopro_rk_upgrade_file(const char *image_path, egopro_progress_cb cb, void *user, const volatile int *cancel_flag);

#ifdef __cplusplus
}
#endif

#endif  // EGOPRO_UPGRADE_API_H
