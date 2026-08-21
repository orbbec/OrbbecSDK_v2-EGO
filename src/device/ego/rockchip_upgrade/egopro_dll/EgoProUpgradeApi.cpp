// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
//
// EgoPro RockUSB upgrade - implementation.
//
// Reuses the Rockchip upgrade-tool source (CRKImage / CRKScan / CRKUsbComm /
// CRKDevice / CRKUpgrade) to flash a full update.img onto an EgoPro device that
// is in RockUSB download mode. The RK source is MIT-licensed and compiled into
// the SDK, which calls this entry through the C ABI in EgoProUpgradeApi.h.
//
// Flow (mirrors the upstream "UF" command):
//   scan -> pick 0x2207/0x350E -> load image -> open comm -> create device ->
//   build the standard upgrade step-set -> Run(). The device is enumerated in
//   LOADER state (bcdUSB bit0 == 1), so CRKUpgrade::Run() automatically skips
//   the MASKROM-only steps via its (uiSupport & UsbType) filter.
//
// VALIDATION NOTES (must be confirmed by compiling against the RK tree and by
// flashing real hardware before production use):
//   [V1] Step-set: CreateUpgradeStep() is the full firmware flow. If the EgoPro
//        should only rewrite partitions (no IDBlock/loader rewrite), switch to
//        CreateDownloadImageStep().
//   [V2] Device type: the scanner reports RKNONE_DEVICE for 0x2207; the real
//        chip type is taken from the image header (CRKImage::SupportDevice).
//   [V3] Cancel is best-effort: the RK API has no mid-transfer abort, so a
//        cancel request is only honored between callbacks.
#include "EgoProUpgradeApi.h"

#include "RKImage.h"
#include "RKDevice.h"
#include "RKUpgrade.h"
#ifdef _WIN32
#include "os\\win\\RKScan.h"
#include "os\\win\\RKComm.h"
#elif __APPLE__
#include "os/mac/RKScan.h"
#include "os/mac/RKComm.h"
#else
#include "os/linux/RKScan.h"
#include "os/linux/RKComm.h"
#endif

#include <string>

namespace {
const uint_16 EGOPRO_VID = 0x2207;
const uint_16 EGOPRO_PID = 0x350E;

// Single upgrade at a time; the SDK serializes update calls per device.
egopro_progress_cb  g_cb     = NULL;
void               *g_user   = NULL;
const volatile int *g_cancel = NULL;

void emit(int percent, const char *message) {
    if(g_cb) {
        g_cb(percent, message, g_user);
    }
}

bool cancelled() {
    return g_cancel != NULL && *g_cancel != 0;
}

// UpgradeStepPromptCB: coarse step transitions.
void stepPrompt(uint_32 /*layer*/, ENUM_UPGRADE_PROMPT prompt, uint_32 /*old*/) {
    switch(prompt) {
    case DOWNLOADIMAGE_START:
        emit(1, "Downloading image...");
        break;
    case DOWNLOADIMAGE_PASS:
        emit(100, "Image download complete");
        break;
    case DOWNLOADIMAGE_FAIL:
        emit(0, "Image download failed");
        break;
    case RESETDEVICE_PASS:
        emit(100, "Device reset; rebooting");
        break;
    default:
        break;
    }
}

// ProgressPromptCB: fine-grained transfer progress.
void progressPrompt(uint_32 /*layer*/, ENUM_PROGRESS_PROMPT prompt, long long totalValue, long long currentValue, ENUM_CALL_STEP /*emCall*/) {
    if(prompt == DOWNLOADIMAGE_PROGRESS && totalValue > 0) {
        int percent = static_cast<int>(currentValue * 100 / totalValue);
        if(percent < 0) {
            percent = 0;
        }
        if(percent > 100) {
            percent = 100;
        }
        emit(percent, "Downloading image...");
    }
}
}  // namespace

extern "C" EGOPRO_API int egopro_rk_upgrade_file(const char *image_path, egopro_progress_cb cb, void *user, const volatile int *cancel_flag) {
    g_cb     = cb;
    g_user   = user;
        g_cancel = cancel_flag;

    if(image_path == NULL) {
        return -1;
    }

    CRKImage   *image   = NULL;
    CRKScan    *scan    = NULL;
    CRKUsbComm *comm    = NULL;
    CRKDevice  *device  = NULL;
    CRKUpgrade *upgrade = NULL;
    int         result  = -1;

    try {

        // Load and verify the update image.
        image = new CRKImage();
        if(image->ObjectInit(image_path) != 0 || image->Error != 0) {
            result = -2;  // bad image
            throw 0;
        }

        // Enumerate RockUSB devices and locate the EgoPro download gadget.
        scan = new CRKScan();
        scan->Search(RKUSB_MASKROM | RKUSB_LOADER);

        STRUCT_RKDEVICE_DESC desc;
        bool                 found = false;
        for(int i = 0; i < scan->DEVICE_COUNTS; ++i) {
            STRUCT_RKDEVICE_DESC d;
            if(scan->GetDevice(d, i) && d.usVid == EGOPRO_VID && d.usPid == EGOPRO_PID) {
                desc  = d;
                found = true;
                break;
            }
        }
        if(!found) {
            result = -3;  // device not present
            throw 0;
        }

        // [V2] The scanner cannot resolve the chip for 0x2207; take it from the image.
        desc.emDeviceType = image->SupportDevice;

        if(cancelled()) {
            result = -9;
            throw 0;
        }

        // Open the RockUSB transport (PIPE00/PIPE01 via rockusb.sys on Windows).
        bool commOk = false;
        comm        = new CRKUsbComm(desc, NULL, commOk);
        if(!commOk) {
            result = -4;  // transport open failed
            throw 0;
        }

        device = CDeviceFactory::CreateDevice(desc);
        if(device == NULL) {
            result = -5;  // unsupported device type
            throw 0;
        }
        device->SetObject(image, comm, NULL);
        // EgoPro Loader accepts writes but does not provide reliable LBA
        // read-back data. Use the upgrade tool''s built-in no-check mode;
        // RockUSB write commands remain checked through their transport status.
        device->NoCheckMode = true;
        device->CallBackPointer = progressPrompt;

        // [V1] Standard full-firmware step-set; MASKROM-only steps are filtered
        // out by Run() because the device is in LOADER state.
        RK_UPGRADE_SET steps;
        if(!CRKUpgrade::CreateUpgradeStep(desc.emDeviceType, steps)) {
            result = -6;  // could not build steps for this chip
            throw 0;
        }

        upgrade                  = new CRKUpgrade(steps, WF_UPGRADE);
        upgrade->CallBackPointer = stepPrompt;
        if(!upgrade->SetObject(device, scan)) {
            result = -7;
            throw 0;
        }

        result = upgrade->Run() ? 0 : -8;
    }
    catch(...) {
        // result already carries the specific error code.
    }

    if(upgrade != NULL) {
        // CRKUpgrade owns the device and scan after SetObject(). The device in
        // turn owns the communication object supplied through SetObject().
        // Do not free any of these again after deleting upgrade.
        delete upgrade;
        upgrade = NULL;
        device  = NULL;
        scan    = NULL;
        comm    = NULL;
    }
    else {
        // Failures before CRKUpgrade takes ownership still need local cleanup.
        if(device != NULL) {
            CDeviceFactory::FreeDevice(device);
            device = NULL;
            comm   = NULL;
        }
        else {
            delete comm;
            comm = NULL;
        }
        delete scan;
        scan = NULL;
    }
    delete image;

    g_cb     = NULL;
    g_user   = NULL;
    g_cancel = NULL;
    return result;
}
