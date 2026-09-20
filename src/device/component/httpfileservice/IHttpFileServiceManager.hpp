// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceComponent.hpp"
#include "libobsensor/h/ObTypes.h"

#include <functional>
#include <string>
#include <vector>

namespace libobsensor {

struct SdcardFileInfo {
    bool     exists = false;
    bool     isDir  = false;
    uint64_t size   = 0;
};

struct SdcardFileEntry {
    std::string name;
    bool        isDir = false;
    uint64_t    size  = 0;
};

// Internal progress callback: state, fileName, percent, md5 (base64, empty when N/A)
using SdcardDownloadProgressCallback =
    std::function<void(ob_sdcard_download_state state, const std::string &fileName,
                       int percent, const std::string &md5)>;

class IHttpFileServiceManager : public IDeviceComponent {
public:
    virtual ~IHttpFileServiceManager() = default;

    /**
     * Query existence and metadata of a path on the SD card.
     * Returns SdcardFileInfo{exists=false} when the firmware reports HTTP 404;
     * throws on communication errors (HTTP 400 / 500 / connection failure).
     */
    virtual SdcardFileInfo getFileInfo(const std::string &path) const = 0;

    /**
     * List the contents of a directory (or the SD card root when path is empty).
     * When recursive is true, sub-directory entries are included; the firmware
     * caps the result at 5000 entries.
     */
    virtual std::vector<SdcardFileEntry> listFiles(const std::string &path, bool recursive) const = 0;

    /**
     * Download a single file from the SD card to a local path.
     * async=false: blocks the calling thread; throws on error.
     * async=true : returns immediately; result reported via callback.
     * Multiple concurrent calls are allowed; each operates independently.
     */
    virtual void downloadToLocal(const std::string &devicePath, const std::string &localPath,
                                 SdcardDownloadProgressCallback cb, bool async) = 0;

    /**
     * Push a file to a remote HTTP URL via the device.
     * async=false: blocks the calling thread until done/failed.
     * async=true : returns immediately; result reported via callback.
     * Multiple concurrent calls are allowed; each operates independently.
     */
    virtual void downloadToUrl(const std::string &devicePath, const std::string &remoteUrl,
                               SdcardDownloadProgressCallback cb, bool async) = 0;

    /**
     * Delete a file or directory from the SD card (directories are removed
     * recursively by the firmware).
     */
    virtual void deleteFile(const std::string &path) const = 0;

    /**
     * Cancel the currently in-progress operation (if any).
     * Safe to call from any thread. The blocked caller will receive an
     * IO exception with message "operation cancelled".
     * Has no effect when no operation is running.
     */
    virtual void cancel() noexcept = 0;
};

}  // namespace libobsensor
