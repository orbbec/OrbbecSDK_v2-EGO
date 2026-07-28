// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <utility>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace {

bool createDirectory(const std::string &path) {
#ifdef _WIN32
    return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

std::string joinPath(const std::string &dir, const std::string &name) {
#ifdef _WIN32
    return dir + "\\" + name;
#else
    return dir + "/" + name;
#endif
}

struct LogSaveContext {
    explicit LogSaveContext(std::string dir) : outputDir(std::move(dir)) {}

    std::string                        outputDir;
    std::map<std::string, std::FILE *> files;
    std::mutex                         mutex;
    std::condition_variable            cond;
    bool                               done  = false;
    bool                               error = false;
};

void closeAllFiles(LogSaveContext &ctx) {
    for(auto &item: ctx.files) {
        if(item.second) {
            std::fclose(item.second);
        }
    }
    ctx.files.clear();
}

void saveEgoFirmwareLogPacket(const char *fileName, OBFwLogPacketState state, const uint8_t *data, uint32_t dataSize, uint32_t totalFileCount,
                              uint32_t completedFileCount, LogSaveContext *ctx) {
    std::lock_guard<std::mutex> lock(ctx->mutex);
    std::string                 name(fileName);

    if(state == OB_FW_LOG_PACKET_BEGIN) {
        auto file = std::fopen(joinPath(ctx->outputDir, name).c_str(), "wb");
        if(file == nullptr) {
            std::cerr << "Open log file failed: " << name << std::endl;
            ctx->error = true;
            ctx->done  = true;
            ctx->cond.notify_one();
            return;
        }
        ctx->files[name] = file;
        std::cout << "Begin: " << name << std::endl;
    }

    auto it = ctx->files.find(name);
    if(data != nullptr && dataSize > 0 && it != ctx->files.end()) {
        if(std::fwrite(data, 1, dataSize, it->second) != dataSize) {
            std::cerr << "Write log file failed: " << name << std::endl;
            ctx->error = true;
            ctx->done  = true;
            ctx->cond.notify_one();
            return;
        }
    }

    if(state == OB_FW_LOG_PACKET_END) {
        if(it != ctx->files.end()) {
            std::fclose(it->second);
            ctx->files.erase(it);
        }
        std::cout << "End: " << name << " (" << completedFileCount << "/" << totalFileCount << ")" << std::endl;
        if(totalFileCount > 0 && completedFileCount >= totalFileCount) {
            ctx->done = true;
            ctx->cond.notify_one();
        }
    }
}

}  // namespace

int main() try {
    const std::string outputDir = "ego_fw_logs";
    if(!createDirectory(outputDir)) {
        std::cerr << "Create output directory failed: " << outputDir << std::endl;
        return EXIT_FAILURE;
    }

    auto context    = std::make_shared<ob::Context>();
    auto deviceList = context->queryDeviceList();
    if(deviceList->getCount() == 0) {
        std::cout << "No device found!" << std::endl;
        return EXIT_FAILURE;
    }

    auto device = deviceList->getDevice(0);
    LogSaveContext saveCtx(outputDir);

    device->startFirmwareLogExport([&saveCtx](const char *fileName, OBFwLogPacketState state, const uint8_t *data, uint32_t dataSize,
                                              uint32_t totalFileCount, uint32_t completedFileCount) {
        saveEgoFirmwareLogPacket(fileName, state, data, dataSize, totalFileCount, completedFileCount, &saveCtx);
    });

    {
        std::unique_lock<std::mutex> lock(saveCtx.mutex);
        saveCtx.cond.wait(lock, [&saveCtx]() { return saveCtx.done; });
    }

    device->stopFirmwareLogExport();
    closeAllFiles(saveCtx);
    std::cout << "Ego firmware log export " << (saveCtx.error ? "failed" : "completed") << std::endl;
    return saveCtx.error ? EXIT_FAILURE : EXIT_SUCCESS;
}
catch(ob::Error &e) {
    std::cerr << "Function: " << e.getFunction() << "\nArgs: " << e.getArgs() << "\nMessage: " << e.what() << "\nStatus: " << e.getStatus()
              << "\nException Type: " << e.getExceptionType() << std::endl;
    return EXIT_FAILURE;
}
catch(const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
