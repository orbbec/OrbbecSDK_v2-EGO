// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IHttpFileServiceManager.hpp"
#include "IDevice.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace libobsensor {

class HttpFileServiceManager : public IHttpFileServiceManager {
public:
    HttpFileServiceManager(IDevice *owner, std::string deviceIp);
    ~HttpFileServiceManager() noexcept override = default;

    IDevice *getOwner() const override;

    SdcardFileInfo               getFileInfo(const std::string &path) const override;
    std::vector<SdcardFileEntry> listFiles(const std::string &path, bool recursive) const override;

    /**
     * Download a single file from the SD card.
     * async=false : blocks the calling thread; errors thrown as exceptions.
     * async=true  : returns immediately; result reported via callback.
     * Multiple concurrent calls are allowed regardless of async mode.
     */
    void downloadToLocal(const std::string &devicePath, const std::string &localPath,
                         SdcardDownloadProgressCallback cb, bool async) override;

    /**
     * Push a file to a remote HTTP URL via the device (async firmware task).
     * async=false : blocks the calling thread until the device reports done/failed.
     * async=true  : returns immediately; result reported via callback.
     */
    void downloadToUrl(const std::string &devicePath, const std::string &remoteUrl,
                       SdcardDownloadProgressCallback cb, bool async) override;

    void deleteFile(const std::string &path) const override;

    /**
     * Cancel ALL in-progress download/upload operations.
     * Thread-safe; safe to call from any thread.
     */
    void cancel() noexcept override;

private:
    // ── HTTP primitives ───────────────────────────────────────────────────

    std::string makeUrl(const std::string &endpoint) const;

    // GET with an absolute transfer timeout.
    // cancelFlag: per-operation flag (may be the shared quickOpCancelFlag_ for quick ops).
    std::string httpGet(const std::string &url, bool allowNotFound, long transferTimeoutMs,
                        const std::atomic<bool> &cancelFlag) const;

    // POST with an absolute transfer timeout.
    std::string httpPost(const std::string &url, long transferTimeoutMs,
                         const std::atomic<bool> &cancelFlag) const;

    // Streaming GET → local file with stall detection and optional progress.
    // outMd5 receives the X-Content-MD5 response header value (base64) if present.
    void httpGetToFile(const std::string &url, const std::string &localPath, int64_t resumeOffset,
                       std::function<void(int64_t, int64_t)> progressCb,
                       const std::atomic<bool> &cancelFlag,
                       std::string &outMd5) const;

    // ── Download / upload helpers ─────────────────────────────────────────

    // Download a single file using the provided per-operation cancel flag.
    // Files larger than kParallelThresholdBytes use downloadFileParallel automatically.
    void downloadSingleFileToLocal(const std::string &devicePath, const std::string &localPath,
                                   SdcardDownloadProgressCallback cb,
                                   const std::atomic<bool> &cancelFlag) const;

    // Download one Range chunk [rangeStart, rangeEnd], write at fileOffset in localPath.
    // Increments *totalReceived as data arrives (used for overall progress reporting).
    void httpGetChunkToFile(const std::string &url, const std::string &localPath,
                            int64_t rangeStart, int64_t rangeEnd, int64_t fileOffset,
                            std::atomic<int64_t> &totalReceived, int64_t totalSize,
                            const std::string &name, SdcardDownloadProgressCallback cb,
                            const std::atomic<bool> &cancelFlag) const;

    // Download fileSize bytes using kParallelChunks parallel Range requests.
    void downloadFileParallel(const std::string &url, const std::string &localPath,
                              int64_t fileSize, const std::string &name,
                              SdcardDownloadProgressCallback cb,
                              const std::atomic<bool> &cancelFlag) const;

    // Files above this threshold use parallel multi-connection download.
    static constexpr int64_t kParallelThresholdBytes = 50LL * 1024 * 1024;  // 50 MB
    static constexpr int      kParallelChunks        = 8;

    // ── Upload helpers (async task protocol) ─────────────────────────────

    struct UploadTaskStatus {
        std::string taskId;
        std::string statusStr;
        std::string fileName;
        int         percent    = 0;
        uint64_t    totalBytes = 0;
        uint64_t    sentBytes  = 0;
        int         doneFiles  = 0;
        int         totalFiles = 0;
        int         failFiles  = 0;
        int         errorCode  = 0;
        std::string errorMsg;
        std::string md5;        // base64-encoded MD5 of the current file (may be empty)
    };

    std::string      initiateUpload(const std::string &devicePath, const std::string &remoteUrl,
                                    const std::atomic<bool> &cancelFlag) const;
    UploadTaskStatus pollUploadStatus(const std::string &taskId,
                                      const std::atomic<bool> &cancelFlag) const;
    void             cancelUploadTask(const std::string &taskId) const noexcept;

    // ── Active-operation tracking (for cancel-all) ────────────────────────

    // Register a new operation's cancel flag; returns it back for chaining.
    std::shared_ptr<std::atomic<bool>> registerOp() const;

    // Remove a completed/failed operation's flag from the tracking list.
    void unregisterOp(const std::shared_ptr<std::atomic<bool>> &flag) const;

private:
    IDevice    *owner_;
    std::string deviceIp_;
    std::string baseUrl_;

    // Cancel flag for quick synchronous ops (stat / list / delete).
    // Not used by download/upload — those have per-operation flags.
    mutable std::atomic<bool> quickOpCancelFlag_{ false };

    // Per-operation cancel flags for all in-progress downloads/uploads.
    // cancel() sets every flag in this list.
    mutable std::mutex                                         opsMutex_;
    mutable std::vector<std::shared_ptr<std::atomic<bool>>>   activeOps_;

    // Upload concurrency limit — firmware rejects > 2 simultaneous upload tasks (HTTP 503).
    // downloadToUrl waits for a slot before calling initiateUpload; releases on completion.
    mutable std::mutex              uploadSlotMutex_;
    mutable std::condition_variable uploadSlotCv_;
    mutable int                     activeUploads_{ 0 };
    static constexpr int            kMaxConcurrentUploads = 2;
};

}  // namespace libobsensor
