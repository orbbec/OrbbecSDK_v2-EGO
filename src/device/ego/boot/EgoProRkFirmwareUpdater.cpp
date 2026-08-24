// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#include "EgoProRkFirmwareUpdater.hpp"
#include "RockusbDllBackend.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cerrno>
#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif
#include <string>
#include <utility>
namespace libobsensor {
namespace {
// Outer RockUSB firmware container ("RKFW") and inner Android image ("RKAF")
// tags. A valid update.img starts with one of these.
bool hasRockusbMagic(const std::vector<uint8_t> &fw) {
    if(fw.size() < 4) {
        return false;
    }
    static const char kRkfw[4] = { 'R', 'K', 'F', 'W' };
    static const char kRkaf[4] = { 'R', 'K', 'A', 'F' };
    return std::equal(kRkfw, kRkfw + 4, fw.begin()) || std::equal(kRkaf, kRkaf + 4, fw.begin());
}

bool hasRockusbMagicFile(const std::string &path) {
    std::ifstream stream(path.c_str(), std::ios::binary);
    char          magic[4] = {};
    return stream.read(magic, sizeof(magic)).gcount() == static_cast<std::streamsize>(sizeof(magic))
           && ((magic[0] == 'R' && magic[1] == 'K' && magic[2] == 'F' && magic[3] == 'W')
               || (magic[0] == 'R' && magic[1] == 'K' && magic[2] == 'A' && magic[3] == 'F'));
}

std::string stageDir() {
#if defined(__ANDROID__)
    // Android application processes cannot write to their current working
    // directory. Use an app-private temporary/cache directory exposed by the
    // runtime environment instead.
    const char *tmpDir = std::getenv("TMPDIR");
    const char *homeDir = std::getenv("HOME");
    std::string dir;
    if(homeDir != NULL && *homeDir != '\0') {
        dir = std::string(homeDir) + "/cache/egopro_upgrade_cache";
    }
    else if(tmpDir != NULL && *tmpDir != '\0') {
        dir = std::string(tmpDir) + "/egopro_upgrade_cache";
    }
    else {
        THROW_IO_EXCEPTION("Failed to locate an app-private cache directory for EgoPro upgrade");
    }
#else
    const std::string dir = "egopro_upgrade_cache";
#endif
#if defined(_WIN32)
    const int rc = _mkdir(dir.c_str());
#else
    const int rc = mkdir(dir.c_str(), 0755);
#endif
    if(rc != 0 && errno != EEXIST) {
        THROW_IO_EXCEPTION("Failed to create EgoPro upgrade cache directory: " + dir);
    }
    return dir;
}
std::string sanitize(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for(char c: s) {
        out.push_back((std::isalnum(static_cast<unsigned char>(c)) != 0) ? c : '_');
    }
    return out;
}

// Stage the in-memory image to a file so the RockUSB backend (CRKImage) can
// open it by path. Returns the written path.
std::string writeTempImage(const std::vector<uint8_t> &fw, const std::string &tag) {
    std::string key = sanitize(tag);
    // RockUSB accepts only a file path. The public SDK has already converted
    // the selected image to raw bytes, so stage it in the application's
    // working directory instead of the system TEMP/TMP directory.
#if defined(_WIN32)
    const char separator = '\\';
#else
    const char separator = '/';
#endif
    std::string path = stageDir() + separator + "egopro_update_" + (key.empty() ? std::string("dev") : key) + ".img";
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if(!ofs) {
        THROW_IO_EXCEPTION("Failed to stage EgoPro update image at " + path);
    }
    ofs.write(reinterpret_cast<const char *>(fw.data()), static_cast<std::streamsize>(fw.size()));
    ofs.close();
    if(!ofs) {
        THROW_IO_EXCEPTION("Failed to write EgoPro update image at " + path);
    }
    return path;
}
}  // namespace

EgoProRkFirmwareUpdater::EgoProRkFirmwareUpdater(std::shared_ptr<const USBSourcePortInfo> info, uint16_t pid)
    : bootPortInfo_(std::move(info)), initialPid_(pid) {}

EgoProRkFirmwareUpdater::~EgoProRkFirmwareUpdater() noexcept {
    cancelled_ = true;
    if(worker_.joinable()) {
        worker_.join();
    }
}

void EgoProRkFirmwareUpdater::update(const std::vector<uint8_t> &fw, DeviceFwUpdateCallback cb, bool async) {
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

void EgoProRkFirmwareUpdater::updateFromPath(const std::string &path, DeviceFwUpdateCallback cb, bool async) {
    if(worker_.joinable()) {
        worker_.join();
    }
    cancelled_ = false;
    worker_ = std::thread([this, path, cb]() { runFromPath(path, cb); });
    if(!async) {
        worker_.join();
    }
}

void EgoProRkFirmwareUpdater::runFromPath(std::string path, DeviceFwUpdateCallback cb) {
    bool verified = false;
    auto report = [&](OBFwUpdateState state, const std::string &message, uint8_t percent) {
        if(cb) {
            try {
                cb(state, message.c_str(), percent);
            }
            catch(const std::exception &e) {
                LOG_ERROR("EgoPro firmware update callback failed: {}", e.what());
            }
            catch(...) {
                LOG_ERROR("EgoPro firmware update callback failed with an unknown exception");
            }
        }
    };
    try {
        report(STAT_VERIFY_IMAGE, "Verifying EgoPro update image...", 0);
        if(!hasRockusbMagicFile(path)) {
            THROW_INVALID_PARAM_EXCEPTION("Not a valid RockUSB update image (missing RKFW/RKAF tag)");
        }
        verified = true;
        report(STAT_VERIFY_SUCCESS, "EgoPro update image verified", 5);
        if(!backend_) {
            backend_ = createDefaultRockusbUpgradeBackend();
        }
        auto progress = [&](int percent, const std::string &message) {
            const int clamped = std::max(0, std::min(100, percent));
            const uint8_t mapped = static_cast<uint8_t>(clamped >= 100 ? 100 : 10 + clamped * 85 / 100);
            report(STAT_FILE_TRANSFER, message.empty() ? std::string("Flashing EgoPro firmware...") : message, mapped);
        };
        report(STAT_IN_PROGRESS, "Flashing EgoPro firmware via RockUSB...", 10);
        backend_->upgrade(path, progress, cancelled_);
        report(STAT_DONE, "EgoPro firmware update completed; device is rebooting", 100);
    }
    catch(const std::exception &e) {
        LOG_ERROR("EgoPro firmware update failed: {}", e.what());
        report(verified ? ERR_OTHER : ERR_VERIFY, e.what(), 0);
    }
    catch(...) {
        const std::string errorMessage = "EgoPro firmware update failed with an unknown exception";
        LOG_ERROR("{}", errorMessage);
        report(verified ? ERR_OTHER : ERR_VERIFY, errorMessage, 0);
    }
}

void EgoProRkFirmwareUpdater::run(std::vector<uint8_t> fw, DeviceFwUpdateCallback cb) {
    bool verified = false;
    auto report   = [&](OBFwUpdateState s, const std::string &m, uint8_t p) {
        if(cb) {
            try {
                cb(s, m.c_str(), p);
            }
            catch(const std::exception &e) {
                LOG_ERROR("EgoPro firmware update callback failed: {}", e.what());
            }
            catch(...) {
                LOG_ERROR("EgoPro firmware update callback failed with an unknown exception");
            }
        }
    };
    std::string stagedPath;
    try {
        report(STAT_VERIFY_IMAGE, "Verifying EgoPro update image...", 0);
        if(!hasRockusbMagic(fw)) {
            THROW_INVALID_PARAM_EXCEPTION("Not a valid RockUSB update image (missing RKFW/RKAF tag)");
        }
        std::string tag = bootPortInfo_ ? (bootPortInfo_->serial.empty() ? bootPortInfo_->uid : bootPortInfo_->serial) : std::string();
        stagedPath      = writeTempImage(fw, tag);
        verified        = true;
        report(STAT_VERIFY_SUCCESS, "EgoPro update image verified", 5);

        if(!backend_) {
            backend_ = createDefaultRockusbUpgradeBackend();
        }
        auto progress = [&](int percent, const std::string &message) {
            int clamped = std::max(0, std::min(100, percent));
            // Reserve 0-10 for verify/staging and 100 for done; map flashing to 10-95.
            uint8_t p = static_cast<uint8_t>(clamped >= 100 ? 100 : 10 + clamped * 85 / 100);
            report(STAT_FILE_TRANSFER, message.empty() ? std::string("Flashing EgoPro firmware...") : message, p);
        };
        report(STAT_IN_PROGRESS, "Flashing EgoPro firmware via RockUSB...", 10);
        backend_->upgrade(stagedPath, progress, cancelled_);

        report(STAT_DONE, "EgoPro firmware update completed; device is rebooting", 100);
    }
    catch(const std::exception &e) {
        LOG_ERROR("EgoPro firmware update failed: {}", e.what());
        report(verified ? ERR_OTHER : ERR_VERIFY, e.what(), 0);
    }
    catch(...) {
        const std::string errorMessage = "EgoPro firmware update failed with an unknown exception";
        LOG_ERROR("{}", errorMessage);
        report(verified ? ERR_OTHER : ERR_VERIFY, errorMessage, 0);
    }
    if(!stagedPath.empty()) {
        std::remove(stagedPath.c_str());
    }
}
}  // namespace libobsensor
