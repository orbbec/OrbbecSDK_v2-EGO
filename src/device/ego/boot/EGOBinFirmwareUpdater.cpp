// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#include "EGOBinFirmwareUpdater.hpp"
#include "EGOBinPackage.hpp"
#include "EGOMd5.hpp"
#include "common/DevicePids.hpp"
#include "Platform.hpp"
#include "protocol/EGOScsiProtocol.hpp"
#include "usb/scsi/ScsiDevicePort.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>
namespace libobsensor {
namespace {
std::string usbTopologyKey(const USBSourcePortInfo &info) {
#if defined(_WIN32)
    return info.uid;
#else
    auto separator = info.url.find_last_of('-');
    if(separator == std::string::npos) {
        return info.url;
    }
    return info.url.substr(0, separator);
#endif
}

void waitForDeviceDigest(uint32_t size, const std::atomic_bool &cancelled) {
    // Keep the timing used by the supplier tool. After DOWNLOAD_END the device
    // calculates MD5 internally; querying the result too early can report a
    // false failure for large partitions.
    auto remaining = std::chrono::milliseconds(size / 20480);
    while(remaining.count() > 0) {
        if(cancelled.load()) {
            THROW_IO_EXCEPTION("EGO firmware update cancelled");
        }
        auto interval = std::min(remaining, std::chrono::milliseconds(100));
        std::this_thread::sleep_for(interval);
        remaining -= interval;
    }
}
}  // namespace
EGOBinFirmwareUpdater::EGOBinFirmwareUpdater(std::shared_ptr<const USBSourcePortInfo> info, uint16_t pid) : bootPortInfo_(std::move(info)), initialPid_(pid) {}
EGOBinFirmwareUpdater::~EGOBinFirmwareUpdater() noexcept {
    cancelled_ = true;
    if(worker_.joinable()) {
        worker_.join();
    }
}
std::shared_ptr<IScsiDevicePort> EGOBinFirmwareUpdater::openPort(std::shared_ptr<const USBSourcePortInfo> info) {
    auto p    = Platform::getInstance()->getUsbSourcePort(info);
    auto scsi = std::dynamic_pointer_cast<IScsiDevicePort>(p);
    if(!scsi) {
        THROW_IO_EXCEPTION("EGO boot source port is not a SCSI port");
    }
    return scsi;
}
std::shared_ptr<const USBSourcePortInfo> EGOBinFirmwareUpdater::waitForUbootPort() {
    for(int i = 0; i < 120 && !cancelled_; ++i) {
        auto infos = Platform::getInstance()->queryUsbSourcePortInfos(false);
        for(auto &base: infos) {
            auto p = std::dynamic_pointer_cast<const USBSourcePortInfo>(base);
            if(!p || p->vid != EGO_BOOT_VID || p->pid != EGO_BOOT_UBOOT_PID) {
                continue;
            }
            bool same = usbTopologyKey(*p) == usbTopologyKey(*bootPortInfo_);
            if(same) {
                return p;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    THROW_DEVICE_UNAVAILABLE_EXCEPTION("Timed out waiting for EGO U-Boot device");
}
void EGOBinFirmwareUpdater::update(const std::vector<uint8_t> &fw, DeviceFwUpdateCallback cb, bool async) {
    if(worker_.joinable()) {
        worker_.join();
    }
    cancelled_ = false;
    auto copy  = fw;
    worker_    = std::thread([this, copy, cb]() { run(copy, cb); });
    if(!async) {
        worker_.join();
    }
}
void EGOBinFirmwareUpdater::run(std::vector<uint8_t> fw, DeviceFwUpdateCallback cb) {
    bool verified = false;
    auto report   = [&](OBFwUpdateState s, const std::string &m, uint8_t p) {
        if(cb) {
            try {
                cb(s, m.c_str(), p);
            }
            catch(const std::exception &e) {
                LOG_ERROR("EGO firmware update callback failed: {}", e.what());
            }
            catch(...) {
                LOG_ERROR("EGO firmware update callback failed with an unknown exception");
            }
        }
    };
    try {
        report(STAT_VERIFY_IMAGE, "Verifying EGO firmware BIN...", 0);
        auto pkg = EGOBinParser::parseAndVerify(fw);
        verified = true;
        report(STAT_VERIFY_SUCCESS, "EGO firmware BIN verified", 5);
        auto     info     = bootPortInfo_;
        uint64_t sent     = 0;
        auto     progress = [&](uint32_t n) {
            sent += n;
            uint64_t transferredProgress = 0;
            if(pkg.totalDownloadSize != 0) {
                transferredProgress = sent * 80 / pkg.totalDownloadSize;
            }
            uint8_t p = uint8_t(10 + transferredProgress);
            report(STAT_FILE_TRANSFER, "Transferring EGO firmware...", p);
        };
        if(initialPid_ == EGO_BOOT_ROM_PID) {
            {
                auto              romPort = openPort(info);
                egoscsi::Protocol romProtocol(romPort);
                report(STAT_START, "Loading EGO USB updater", 8);
                auto     inquiry = romProtocol.inquiry();
                uint32_t iplSize = pkg.usbUpdater.size;
                if(inquiry[17] == 1) {
                    iplSize = 30 * 1024;
                }
                if(iplSize > pkg.usbUpdater.size || uint64_t(pkg.usbUpdater.offset) + iplSize > pkg.payloadSize) {
                    THROW_INVALID_PARAM_EXCEPTION("EGO USB updater segment is too small for ROM revision");
                }
                romProtocol.download(fw.data() + pkg.usbUpdater.offset, iplSize, 1024, cancelled_, progress);
                auto digest = egoMd5(fw.data() + pkg.uboot.offset, pkg.uboot.size);
                romProtocol.sendLoadInfo(0xffffffff, pkg.uboot.size, digest);
                romProtocol.download(fw.data() + pkg.uboot.offset, pkg.uboot.size, egoscsi::DOWNLOAD_CHUNK_SIZE, cancelled_, progress);
                waitForDeviceDigest(pkg.uboot.size, cancelled_);
                romProtocol.checkResult();
            }
            info = waitForUbootPort();
        }
        auto              port = openPort(info);
        egoscsi::Protocol protocol(port);
        report(STAT_IN_PROGRESS, "Executing EGO U-Boot update script", 10);
        protocol.runCommand("setenv ota_upgrade_status 0", false);
        for(const auto &cmd: pkg.commands) {
            if(cancelled_) {
                THROW_IO_EXCEPTION("EGO firmware update cancelled");
            }
            if(cmd.type == EGOUpgradeCommand::UsbLoad) {
                auto digest = egoMd5(fw.data() + cmd.offset, cmd.size);
                protocol.sendLoadInfo(cmd.address, cmd.size, digest);
                protocol.download(fw.data() + cmd.offset, cmd.size, egoscsi::DOWNLOAD_CHUNK_SIZE, cancelled_, progress);
                waitForDeviceDigest(cmd.size, cancelled_);
                protocol.checkResult();
            }
            else {
                protocol.runCommand(cmd.text);
                if(cmd.text == "reset" || cmd.text == "poweroff") {
                    report(STAT_DONE, "EGO firmware update completed; device is rebooting", 100);
                    return;
                }
            }
        }
        report(STAT_DONE, "EGO firmware update completed", 100);
    }
    catch(const std::exception &e) {
        LOG_ERROR("EGO firmware update failed: {}", e.what());
        if(verified) {
            report(ERR_OTHER, e.what(), 0);
        }
        else {
            report(ERR_VERIFY, e.what(), 0);
        }
    }
    catch(...) {
        const std::string errorMessage = "EGO firmware update failed with an unknown exception";
        LOG_ERROR("{}", errorMessage);
        if(verified) {
            report(ERR_OTHER, errorMessage, 0);
        }
        else {
            report(ERR_VERIFY, errorMessage, 0);
        }
    }
}
}  // namespace libobsensor
