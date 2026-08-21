// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#include "RockusbDllBackend.hpp"
#include "exception/ObException.hpp"
#include <string>

#if !defined(OS_IOS)
#include "EgoProUpgradeApi.h"
#endif

namespace libobsensor {
namespace {
// Context threaded through the C progress callback.
struct ProgressContext {
    const IRockusbUpgradeBackend::ProgressCallback *cb;
    const std::atomic_bool                         *cancelled;
    volatile int                                   *cancelFlag;
};

void progressTrampoline(int percent, const char *message, void *user) {
    auto *ctx = static_cast<ProgressContext *>(user);
    if(ctx == nullptr) {
        return;
    }
    // Propagate a cancel request to the upgrade tool, which polls the flag.
    if(ctx->cancelled != nullptr && ctx->cancelFlag != nullptr && ctx->cancelled->load()) {
        *ctx->cancelFlag = 1;
    }
    if(ctx->cb != nullptr && *ctx->cb) {
        (*ctx->cb)(percent, message != nullptr ? std::string(message) : std::string());
    }
}
}  // namespace

RockusbDllBackend::RockusbDllBackend() = default;

RockusbDllBackend::~RockusbDllBackend() noexcept = default;

void RockusbDllBackend::upgrade(const std::string &imagePath, const ProgressCallback &progress, const std::atomic_bool &cancelled) {
#if defined(OS_IOS)
    (void)imagePath;
    (void)progress;
    (void)cancelled;
    THROW_UNSUPPORTED_OPERATION_EXCEPTION("EgoPro RockUSB firmware upgrade is not supported on iOS");
#else
    volatile int    cancelFlag = cancelled.load() ? 1 : 0;
    ProgressContext ctx{ &progress, &cancelled, &cancelFlag };

    // The Rockchip upgrade-tool sources are linked into the SDK; call directly.
    int rc = egopro_rk_upgrade_file(imagePath.c_str(), &progressTrampoline, &ctx, &cancelFlag);
    if(cancelled.load()) {
        THROW_IO_EXCEPTION("EgoPro firmware update cancelled");
    }
    if(rc != 0) {
        THROW_IO_EXCEPTION("EgoPro RockUSB upgrade failed with code " + std::to_string(rc));
    }
#endif
}

std::unique_ptr<IRockusbUpgradeBackend> createDefaultRockusbUpgradeBackend() {
    return std::unique_ptr<IRockusbUpgradeBackend>(new RockusbDllBackend());
}
}  // namespace libobsensor
