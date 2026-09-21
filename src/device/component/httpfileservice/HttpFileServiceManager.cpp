// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "HttpFileServiceManager.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"

#include <curl/curl.h>
#include <json/json.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <sstream>
#include <thread>
#include <utility>

namespace libobsensor {

// C++11 requires an out-of-class definition when this constexpr member is
// odr-used (for example, when passed by reference through the logger).
constexpr int HttpFileServiceManager::kParallelChunks;

// ── libcurl helpers ────────────────────────────────────────────────────────

namespace {

size_t writeToString(char *ptr, size_t size, size_t nmemb, void *userdata) {
    static_cast<std::string *>(userdata)->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t writeToFile(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *file = static_cast<std::ofstream *>(userdata);
    file->write(ptr, static_cast<std::streamsize>(size * nmemb));
    return file->good() ? size * nmemb : 0;
}

// Capture X-Content-MD5 response header (download response → file MD5).
size_t headerCallback(char *buffer, size_t size, size_t nitems, void *userdata) {
    const size_t total = size * nitems;
    auto        *md5   = static_cast<std::string *>(userdata);
    const std::string line(buffer, total);
    const std::string prefix = "X-Content-MD5:";
    if(line.size() > prefix.size() &&
       line.substr(0, prefix.size()) == prefix) {
        size_t s = line.find_first_not_of(" \t", prefix.size());
        size_t e = line.find_last_not_of(" \t\r\n");
        if(s != std::string::npos && e >= s)
            *md5 = line.substr(s, e - s + 1);
    }
    return total;
}

struct ProgressCtx {
    std::function<void(int64_t, int64_t)>  cb;
    const std::atomic<bool>               *cancelFlag;  // non-owning, per-operation
};

int curlProgressCb(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                   curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    const auto *ctx = static_cast<const ProgressCtx *>(clientp);
    if(!ctx) return 0;
    if(ctx->cancelFlag && ctx->cancelFlag->load(std::memory_order_relaxed)) return 1;
    if(ctx->cb) ctx->cb(static_cast<int64_t>(dlnow), static_cast<int64_t>(dltotal));
    return 0;
}

std::string urlEncode(CURL *curl, const std::string &value) {
    char       *enc = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
    std::string result(enc ? enc : value);
    curl_free(enc);
    return result;
}

bool parseJson(const std::string &body, Json::Value &root, std::string &errors) {
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    std::istringstream input(body);
    return Json::parseFromStream(builder, input, &root, &errors);
}

void checkJsonCode(const std::string &body, const std::string &context) {
    Json::Value root;
    std::string errors;
    if(!parseJson(body, root, errors))
        THROW_IO_EXCEPTION("HTTP file service: invalid JSON in " + context + ": " + errors);
    const int code = root.get("code", 0).asInt();
    if(code != 0)
        THROW_IO_EXCEPTION("HTTP file service error in " + context + " (code=" +
                           std::to_string(code) + "): " + root.get("msg", "unknown error").asString());
}

void attachCancelAndProgress(CURL *curl, ProgressCtx *ctx) {
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlProgressCb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, ctx);
}

// ── FileServiceError: typed exception that carries a specific download-state ──
// Thrown by throwCurlError / checkHttpCode so catch blocks can set the exact
// ob_sdcard_download_state without fragile string matching.
struct FileServiceError : public std::exception {
    ob_sdcard_download_state state;
    std::string              msg;
    FileServiceError(ob_sdcard_download_state s, std::string m)
        : state(s), msg(std::move(m)) {}
    const char *what() const noexcept override { return msg.c_str(); }
};

// Helper: extract state from current exception (for catch(...) blocks)
static ob_sdcard_download_state stateFromCurrentException(
        const std::atomic<bool> &cancelFlag) noexcept {
    if(cancelFlag.load(std::memory_order_relaxed))
        return OB_SDCARD_DOWNLOAD_CANCELLED;
    try {
        auto p = std::current_exception();
        if(p) std::rethrow_exception(p);
    }
    catch(const FileServiceError &e) { return e.state; }
    catch(...) {}
    return OB_SDCARD_DOWNLOAD_FAILED;
}

[[noreturn]] void throwCurlError(CURLcode res, const std::string &url) {
    if(res == CURLE_ABORTED_BY_CALLBACK)
        throw FileServiceError(OB_SDCARD_DOWNLOAD_CANCELLED,
                               "HTTP file service: operation cancelled");
    if(res == CURLE_OPERATION_TIMEDOUT)
        throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_TIMEOUT,
                               "HTTP file service: timed out for " + url);
    throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_NETWORK,
                           "HTTP file service: request failed for " + url +
                           ": " + curl_easy_strerror(res));
}

void checkHttpCode(long httpCode, const std::string &responseBody, const std::string &url) {
    if(httpCode >= 200 && httpCode < 300) return;
    Json::Value root;
    std::string errors;
    std::string msg = "HTTP " + std::to_string(httpCode);
    if(parseJson(responseBody, root, errors))
        msg += ": " + root.get("msg", responseBody).asString();

    if(httpCode == 404)
        throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_NOT_FOUND,
                               "HTTP file service: path not found (" + msg + ")");
    if(httpCode == 503)
        throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_BUSY,
                               "HTTP file service: upload task slots full (" + msg + ")");
    if(httpCode == 409)
        throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_CAMERA_STREAMING,
                               "HTTP file service: camera streaming, operation not allowed (" + msg + ")");
    if(httpCode >= 500)
        throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_SERVER,
                               "HTTP file service: " + url + " server error " + msg);
    throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED,
                           "HTTP file service: " + url + " returned " + msg);
}

constexpr long kConnectTimeoutMs         = 5000;
constexpr long kDownloadConnectTimeoutMs = 3000;
constexpr long kQuickOpTimeoutMs     = 30000;
constexpr long kUploadPollTimeoutMs  = 30000;   // 30 s — device may be slow to respond while uploading
constexpr long kLowSpeedLimitBps     = 1;
constexpr long kLowSpeedTimeSec      = 30;    // fail quickly when transfer makes no progress
constexpr int  kFileServicePort      = 8080;
constexpr int  kUploadPollIntervalMs = 500;
constexpr int  kMaxDownloadRetries   = 1;     // one resume-capable retry on timeout/network error
constexpr int  kRetryBaseDelaySec    = 2;

// ── Write callback context for a single parallel chunk ─────────────────────
struct ChunkWriteCtx {
    std::ofstream         *file;
    std::atomic<int64_t>  *totalReceived;
    int64_t                totalSize;
    const std::string     *name;
    SdcardDownloadProgressCallback *cb;
    int                    lastPercent;

    ChunkWriteCtx(std::ofstream *outputFile, std::atomic<int64_t> *received,
                  int64_t expectedSize, const std::string *fileName,
                  SdcardDownloadProgressCallback *progressCallback)
        : file(outputFile), totalReceived(received), totalSize(expectedSize),
          name(fileName), cb(progressCallback), lastPercent(-1) {}
};

static size_t chunkWriteCb(char *ptr, size_t sz, size_t nmemb, void *ud) {
    auto  *c     = static_cast<ChunkWriteCtx *>(ud);
    size_t bytes = sz * nmemb;
    c->file->write(ptr, static_cast<std::streamsize>(bytes));
    if(!c->file->good()) return 0;

    int64_t done = c->totalReceived->fetch_add(static_cast<int64_t>(bytes),
                                                std::memory_order_relaxed)
                   + static_cast<int64_t>(bytes);
    if(*c->cb && c->totalSize > 0) {
        int pct = static_cast<int>(done * 100 / c->totalSize);
        pct = (std::max)(0, (std::min)(99, pct));
        if(pct != c->lastPercent && pct % 5 == 0) {
            c->lastPercent = pct;
            (*c->cb)(OB_SDCARD_DOWNLOAD_TRANSFERRING, *c->name, pct, "");
        }
    }
    return bytes;
}

}  // namespace

// ── HttpFileServiceManager ─────────────────────────────────────────────────

HttpFileServiceManager::HttpFileServiceManager(IDevice *owner, std::string deviceIp)
    : owner_(owner), deviceIp_(std::move(deviceIp)) {
    std::ostringstream ss;
    ss << "http://" << deviceIp_ << ":" << kFileServicePort;
    baseUrl_ = ss.str();
    LOG_DEBUG("HttpFileServiceManager: base URL = {}", baseUrl_);
}

IDevice *HttpFileServiceManager::getOwner() const { return owner_; }
std::string HttpFileServiceManager::makeUrl(const std::string &ep) const { return baseUrl_ + ep; }

// ── Active-operation tracking ──────────────────────────────────────────────

std::shared_ptr<std::atomic<bool>> HttpFileServiceManager::registerOp() const {
    auto flag = std::make_shared<std::atomic<bool>>(false);
    std::lock_guard<std::mutex> lock(opsMutex_);
    activeOps_.push_back(flag);
    return flag;
}

void HttpFileServiceManager::unregisterOp(const std::shared_ptr<std::atomic<bool>> &flag) const {
    std::lock_guard<std::mutex> lock(opsMutex_);
    activeOps_.erase(std::remove(activeOps_.begin(), activeOps_.end(), flag), activeOps_.end());
}

void HttpFileServiceManager::cancel() noexcept {
    // Cancel all in-progress download/upload operations.
    {
        std::lock_guard<std::mutex> lock(opsMutex_);
        for(const auto &flag : activeOps_)
            flag->store(true, std::memory_order_relaxed);
    }
    // Also cancel any quick synchronous op that might be running.
    quickOpCancelFlag_.store(true, std::memory_order_relaxed);
    // Wake any threads waiting for an upload slot so they can detect the cancel flag.
    uploadSlotCv_.notify_all();
    LOG_DEBUG("HttpFileServiceManager: cancel requested");
}

// ── httpGet ────────────────────────────────────────────────────────────────

std::string HttpFileServiceManager::httpGet(const std::string &url, bool allowNotFound,
                                             long transferTimeoutMs,
                                             const std::atomic<bool> &cancelFlag) const {
    CURL *curl = curl_easy_init();
    if(!curl) THROW_IO_EXCEPTION("HttpFileServiceManager: curl_easy_init() failed");

    std::string responseBody;
    long        httpCode = 0;
    ProgressCtx ctx{ nullptr, &cancelFlag };

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, kConnectTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, transferTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    attachCancelAndProgress(curl, &ctx);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if(res != CURLE_OK) throwCurlError(res, url);
    if(httpCode == 404 && allowNotFound) return "";
    checkHttpCode(httpCode, responseBody, url);
    return responseBody;
}

// ── httpPost ───────────────────────────────────────────────────────────────

std::string HttpFileServiceManager::httpPost(const std::string &url, long transferTimeoutMs,
                                              const std::atomic<bool> &cancelFlag) const {
    CURL *curl = curl_easy_init();
    if(!curl) THROW_IO_EXCEPTION("HttpFileServiceManager: curl_easy_init() failed");

    std::string responseBody;
    long        httpCode = 0;
    ProgressCtx ctx{ nullptr, &cancelFlag };

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, kConnectTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, transferTimeoutMs);
    attachCancelAndProgress(curl, &ctx);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if(res != CURLE_OK) throwCurlError(res, url);
    checkHttpCode(httpCode, responseBody, url);
    return responseBody;
}

// ── httpGetToFile ──────────────────────────────────────────────────────────

void HttpFileServiceManager::httpGetToFile(const std::string &url, const std::string &localPath,
                                            int64_t resumeOffset,
                                            std::function<void(int64_t, int64_t)> progressCb,
                                            const std::atomic<bool> &cancelFlag,
                                            std::string &outMd5) const {
    const auto openMode = (resumeOffset > 0) ? (std::ios::binary | std::ios::app)
                                              : (std::ios::binary | std::ios::trunc);
    std::ofstream file(localPath, openMode);
    if(!file.is_open())
        THROW_IO_EXCEPTION("HTTP file service: cannot open local file: " + localPath);

    CURL *curl = curl_easy_init();
    if(!curl) THROW_IO_EXCEPTION("HttpFileServiceManager: curl_easy_init() failed");

    ProgressCtx ctx{ std::move(progressCb), &cancelFlag };
    outMd5.clear();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 1024 * 1024L);  // 1 MB — minimises write-callback overhead for large files
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &outMd5);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, kDownloadConnectTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, kLowSpeedLimitBps);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME,  kLowSpeedTimeSec);
    if(resumeOffset > 0)
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(resumeOffset));
    attachCancelAndProgress(curl, &ctx);

    const CURLcode res      = curl_easy_perform(curl);
    long           httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    file.close();

    if(res != CURLE_OK) throwCurlError(res, url);
    if(httpCode == 416) {
        // HTTP 416 Range Not Satisfiable: the local file is already complete
        // (resumeOffset == file size on device). Treat as a successful download.
        LOG_DEBUG("HttpFileServiceManager: local file already complete, skipping download: {}", url);
        return;
    }
    if(httpCode != 200 && httpCode != 206) {
        const std::string msg = "HTTP file service: download returned HTTP " + std::to_string(httpCode) + " from " + url;
        if(httpCode == 404)
            throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_NOT_FOUND, msg);
        if(httpCode == 409)
            throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_CAMERA_STREAMING, msg);
        if(httpCode == 503)
            throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_BUSY, msg);
        if(httpCode >= 500)
            throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_SERVER, msg);
        throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED, msg);
    }
}

// ── downloadSingleFileToLocal ─────────────────────────────────────────────

// ── httpGetChunkToFile ────────────────────────────────────────────────────

void HttpFileServiceManager::httpGetChunkToFile(const std::string &url,
                                                  const std::string &localPath,
                                                  int64_t rangeStart, int64_t rangeEnd,
                                                  int64_t fileOffset,
                                                  std::atomic<int64_t> &totalReceived,
                                                  int64_t totalSize,
                                                  const std::string &name,
                                                  SdcardDownloadProgressCallback cb,
                                                  const std::atomic<bool> &cancelFlag) const {
    // Each chunk opens the pre-allocated file with its own handle and seeks to its offset.
    std::ofstream file(localPath, std::ios::binary | std::ios::in | std::ios::out);
    if(!file.is_open())
        throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED,
                               "HTTP file service: cannot open file for chunk: " + localPath);
    file.seekp(fileOffset);

    CURL *curl = curl_easy_init();
    if(!curl) throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED,
                                     "HttpFileServiceManager: curl_easy_init() failed");

    const std::string range = std::to_string(rangeStart) + "-" + std::to_string(rangeEnd);

    ChunkWriteCtx wCtx{ &file, &totalReceived, totalSize, &name, &cb };
    ProgressCtx   pCtx{ nullptr, &cancelFlag };

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 1024 * 1024L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, chunkWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &wCtx);
    curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, kDownloadConnectTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, kLowSpeedLimitBps);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME,  kLowSpeedTimeSec);
    attachCancelAndProgress(curl, &pCtx);

    const CURLcode res      = curl_easy_perform(curl);
    long           httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    file.close();

    if(res != CURLE_OK) throwCurlError(res, url);
    if(httpCode != 200 && httpCode != 206) {
        const std::string msg = "HTTP file service: chunk returned HTTP " + std::to_string(httpCode);
        if(httpCode == 404)
            throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_NOT_FOUND, msg);
        if(httpCode == 409)
            throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_CAMERA_STREAMING, msg);
        if(httpCode == 503)
            throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_BUSY, msg);
        if(httpCode >= 500)
            throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_SERVER, msg);
        throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED, msg);
    }
}

// ── downloadFileParallel ──────────────────────────────────────────────────

void HttpFileServiceManager::downloadFileParallel(const std::string &url,
                                                   const std::string &localPath,
                                                   int64_t fileSize,
                                                   const std::string &name,
                                                   SdcardDownloadProgressCallback cb,
                                                   const std::atomic<bool> &cancelFlag) const {
    LOG_INFO("HttpFileServiceManager: parallel download {} chunks, file='{}', size={} bytes",
             kParallelChunks, name, fileSize);
    fprintf(stderr, "[HttpFileServiceManager] parallel download: %d chunks, file='%s', size=%lld bytes\n",
            kParallelChunks, name.c_str(), static_cast<long long>(fileSize));

    // Pre-allocate the output file so all threads can seek and write independently.
    {
        std::ofstream pre(localPath, std::ios::binary | std::ios::trunc);
        if(!pre.is_open())
            throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED,
                                   "HTTP file service: cannot create file: " + localPath);
        pre.seekp(fileSize - 1);
        char z = 0;
        pre.write(&z, 1);
    }

    const int64_t chunkSize = (fileSize + kParallelChunks - 1) / kParallelChunks;

    std::atomic<int64_t>            totalReceived{ 0 };
    std::vector<std::thread>        threads;
    std::vector<std::exception_ptr> errors(static_cast<size_t>(kParallelChunks));

    for(int i = 0; i < kParallelChunks; ++i) {
        const int64_t start = static_cast<int64_t>(i) * chunkSize;
        const int64_t end   = (std::min)(start + chunkSize - 1, fileSize - 1);
        if(start >= fileSize) break;

        threads.emplace_back([this, i, start, end, &url, &localPath, &totalReceived,
                               fileSize, &name, cb, &cancelFlag, &errors]() {
            try {
                httpGetChunkToFile(url, localPath, start, end, start,
                                   totalReceived, fileSize, name, cb, cancelFlag);
            }
            catch(...) {
                errors[static_cast<size_t>(i)] = std::current_exception();
            }
        });
    }

    for(auto &t : threads)
        if(t.joinable()) t.join();

    // Re-throw the first error encountered.
    for(auto &e : errors)
        if(e) std::rethrow_exception(e);
}

// ── downloadSingleFileToLocal ─────────────────────────────────────────────

void HttpFileServiceManager::downloadSingleFileToLocal(const std::string &devicePath,
                                                        const std::string &localPath,
                                                        SdcardDownloadProgressCallback cb,
                                                        const std::atomic<bool> &cancelFlag) const {
    CURL       *curl        = curl_easy_init();
    std::string encodedPath = curl ? urlEncode(curl, devicePath) : devicePath;
    if(curl) curl_easy_cleanup(curl);

    const std::string url  = makeUrl("/api/fs/download?path=") + encodedPath;
    const std::string name = devicePath.substr(devicePath.find_last_of('/') + 1);

    // Check whether a partial local file already exists (for resume).
    int64_t resumeOffset = 0;
    {
        std::ifstream existing(localPath, std::ios::binary | std::ios::ate);
        if(existing.is_open())
            resumeOffset = static_cast<int64_t>(existing.tellg());
    }

    // Use parallel multi-connection download for large files without a partial resume.
    // Resume is not supported in parallel mode — fall back to single connection.
    if(resumeOffset == 0) {
        // Query file size to decide whether to use parallel download.
        const auto fileInfo = getFileInfo(devicePath);
        if(fileInfo.exists && !fileInfo.isDir &&
           static_cast<int64_t>(fileInfo.size) >= kParallelThresholdBytes) {
            if(cb) cb(OB_SDCARD_DOWNLOAD_TRANSFERRING, name, 0, "");
            try {
                downloadFileParallel(url, localPath, static_cast<int64_t>(fileInfo.size),
                                     name, cb, cancelFlag);
            }
            catch(...) {
                // The pre-allocated output file is partially filled and cannot be resumed.
                // Remove it so the next attempt starts a fresh parallel download.
                std::remove(localPath.c_str());
                throw;
            }
            if(cb) cb(OB_SDCARD_DOWNLOAD_DONE, name, 100, "");
            return;
        }
    }

    // Single-connection download.
    // Automatically retries on transient timeout/network errors; each retry re-reads
    // the local file size so any partial data is resumed via HTTP Range.
    for(int attempt = 0; ; ++attempt) {
        // Re-read resume offset at the start of every attempt.
        {
            std::ifstream existing(localPath, std::ios::binary | std::ios::ate);
            resumeOffset = existing.is_open() ? static_cast<int64_t>(existing.tellg()) : 0;
        }

        auto progressCb = [&, resumeOffset](int64_t received, int64_t total) {
            if(!cb) return;
            int percent = -1;
            if(total > 0) {
                const int64_t full = resumeOffset + received;
                const int64_t tot  = resumeOffset + total;
                percent = static_cast<int>(full * 100 / tot);
                percent = (std::max)(0, (std::min)(100, percent));
            }
            cb(OB_SDCARD_DOWNLOAD_TRANSFERRING, name, percent, "");
        };

        if(cb) cb(OB_SDCARD_DOWNLOAD_TRANSFERRING, name, resumeOffset > 0 ? -1 : 0, "");
        std::string md5;
        try {
            httpGetToFile(url, localPath, resumeOffset, progressCb, cancelFlag, md5);
            if(cb) cb(OB_SDCARD_DOWNLOAD_DONE, name, 100, md5);
            return;
        }
        catch(const FileServiceError &e) {
            const bool retryable = (e.state == OB_SDCARD_DOWNLOAD_FAILED_TIMEOUT ||
                                    e.state == OB_SDCARD_DOWNLOAD_FAILED_NETWORK);
            if(retryable && attempt < kMaxDownloadRetries &&
               !cancelFlag.load(std::memory_order_relaxed)) {
                const int delaySec = kRetryBaseDelaySec * (1 << attempt);
                LOG_WARN("HttpFileServiceManager: download {} on attempt {}/{}, retry in {}s: {}",
                         static_cast<int>(e.state), attempt + 1, kMaxDownloadRetries, delaySec, e.what());
                std::this_thread::sleep_for(std::chrono::seconds(delaySec));
                continue;
            }
            throw;
        }
    }
}

// ── downloadToLocal ────────────────────────────────────────────────────────

void HttpFileServiceManager::downloadToLocal(const std::string &devicePath,
                                              const std::string &localPath,
                                              SdcardDownloadProgressCallback cb, bool async) {
    auto cancelFlag = registerOp();

    // Core work — always re-throws so the caller (sync or async wrapper) decides how to handle.
    auto doWork = [this, devicePath, localPath, cb, cancelFlag]() {
        try {
            downloadSingleFileToLocal(devicePath, localPath, cb, *cancelFlag);
        }
        catch(...) {
            const auto st = stateFromCurrentException(*cancelFlag);
            if(cb) cb(st, devicePath, -1, "");
            unregisterOp(cancelFlag);
            throw;
        }
        unregisterOp(cancelFlag);
    };

    if(async) {
        // Async: start background thread, swallow exception (already reported via callback).
        std::thread([doWork]() {
            try { doWork(); }
            catch(...) { /* exception already delivered to callback */ }
        }).detach();
    }
    else {
        // Sync: block and let exception propagate to the caller.
        doWork();
    }
}

// ── initiateUpload ─────────────────────────────────────────────────────────

std::string HttpFileServiceManager::initiateUpload(const std::string &devicePath,
                                                    const std::string &remoteUrl,
                                                    const std::atomic<bool> &cancelFlag) const {
    CURL       *curl        = curl_easy_init();
    std::string encodedPath = curl ? urlEncode(curl, devicePath) : devicePath;
    std::string encodedUrl  = curl ? urlEncode(curl, remoteUrl)  : remoteUrl;
    if(curl) curl_easy_cleanup(curl);

    const std::string postUrl = makeUrl("/api/fs/upload?path=") + encodedPath + "&url=" + encodedUrl;
    const std::string body    = httpPost(postUrl, kQuickOpTimeoutMs, cancelFlag);

    Json::Value root;
    std::string errors;
    if(!parseJson(body, root, errors))
        THROW_IO_EXCEPTION("HTTP file service: invalid JSON from upload initiate: " + errors);
    if(root.get("code", -1).asInt() != 0)
        THROW_IO_EXCEPTION("HTTP file service: upload initiate failed: " +
                           root.get("msg", "").asString());

    const std::string taskId = root.get("taskId", "").asString();
    if(taskId.empty())
        THROW_IO_EXCEPTION("HTTP file service: upload initiate returned empty taskId");
    return taskId;
}

// ── pollUploadStatus ──────────────────────────────────────────────────────

HttpFileServiceManager::UploadTaskStatus
HttpFileServiceManager::pollUploadStatus(const std::string &taskId,
                                          const std::atomic<bool> &cancelFlag) const {
    CURL       *curl          = curl_easy_init();
    std::string encodedTaskId = curl ? urlEncode(curl, taskId) : taskId;
    if(curl) curl_easy_cleanup(curl);

    const std::string url  = makeUrl("/api/fs/upload/status?taskId=") + encodedTaskId;
    const std::string body = httpGet(url, false, kUploadPollTimeoutMs, cancelFlag);

    Json::Value root;
    std::string errors;
    if(!parseJson(body, root, errors))
        THROW_IO_EXCEPTION("HTTP file service: invalid JSON from upload/status: " + errors);

    UploadTaskStatus s;
    s.taskId     = root.get("taskId",    "").asString();
    s.statusStr  = root.get("status",    "").asString();
    s.fileName   = root.get("fileName",  "").asString();
    s.percent    = root.get("percent",   0).asInt();
    s.totalBytes = static_cast<uint64_t>(root.get("totalBytes", 0u).asUInt64());
    s.sentBytes  = static_cast<uint64_t>(root.get("sentBytes",  0u).asUInt64());
    s.doneFiles  = root.get("doneFiles",  0).asInt();
    s.totalFiles = root.get("totalFiles", 0).asInt();
    s.failFiles  = root.get("failFiles",  0).asInt();
    s.md5        = root.get("md5",        "").asString();
    const Json::Value &err = root["error"];
    if(err.isObject()) {
        s.errorCode = err.get("code", 0).asInt();
        s.errorMsg  = err.get("msg",  "").asString();
    }
    return s;
}

// ── cancelUploadTask ──────────────────────────────────────────────────────

void HttpFileServiceManager::cancelUploadTask(const std::string &taskId) const noexcept {
    try {
        CURL *curl = curl_easy_init();
        if(!curl) return;

        std::string encodedTaskId = urlEncode(curl, taskId);
        const std::string postUrl = makeUrl("/api/fs/upload/cancel?taskId=") + encodedTaskId;

        std::string responseBody;
        curl_easy_setopt(curl, CURLOPT_URL, postUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, kConnectTimeoutMs);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, kQuickOpTimeoutMs);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        LOG_DEBUG("HttpFileServiceManager: cancelled upload task {}", taskId);
    }
    catch(const std::exception &e) {
        LOG_WARN("HttpFileServiceManager: failed to cancel upload task {}: {}", taskId, e.what());
    }
}

// ── downloadToUrl ──────────────────────────────────────────────────────────

void HttpFileServiceManager::downloadToUrl(const std::string &devicePath,
                                            const std::string &remoteUrl,
                                            SdcardDownloadProgressCallback cb, bool async) {
    auto cancelFlag = registerOp();

    const auto slash = devicePath.find_last_of('/');
    const std::string name = (slash == std::string::npos) ? devicePath : devicePath.substr(slash + 1);

    auto work = [this, devicePath, remoteUrl, name, cb, cancelFlag]() {
        if(cb) cb(OB_SDCARD_DOWNLOAD_TRANSFERRING, name, 0, "");

        // Wait for an upload slot — firmware rejects > kMaxConcurrentUploads with HTTP 503.
        // Also wakes when cancel() is called so this thread can exit immediately.
        {
            std::unique_lock<std::mutex> lock(uploadSlotMutex_);
            uploadSlotCv_.wait(lock, [this, &cancelFlag] {
                return activeUploads_ < kMaxConcurrentUploads ||
                       cancelFlag->load(std::memory_order_relaxed);
            });
            if(cancelFlag->load(std::memory_order_relaxed)) {
                // Cancelled while waiting — no slot acquired, just report and exit.
                if(cb) cb(OB_SDCARD_DOWNLOAD_CANCELLED, name, -1, "");
                unregisterOp(cancelFlag);
                THROW_IO_EXCEPTION("HTTP file service: operation cancelled");
            }
            ++activeUploads_;
        }

        // Helper: release the upload slot and wake waiting threads.
        auto releaseSlot = [this]() noexcept {
            {
                std::lock_guard<std::mutex> l(uploadSlotMutex_);
                --activeUploads_;
            }
            uploadSlotCv_.notify_all();
        };

        std::string taskId;
        try {
            taskId = initiateUpload(devicePath, remoteUrl, *cancelFlag);
        }
        catch(...) {
            releaseSlot();
            const auto st = stateFromCurrentException(*cancelFlag);
            if(cb) cb(st, name, -1, "");
            unregisterOp(cancelFlag);
            throw;
        }
        LOG_DEBUG("HttpFileServiceManager: upload task started, taskId={}", taskId);

        try {
            constexpr int kMaxPollRetries = 5;  // retry poll HTTP timeouts before giving up
            int pollFailures = 0;
            while(true) {
                if(cancelFlag->load(std::memory_order_relaxed)) {
                    cancelUploadTask(taskId);
                    throw FileServiceError(OB_SDCARD_DOWNLOAD_CANCELLED,
                                           "HTTP file service: operation cancelled");
                }

                UploadTaskStatus status;
                try {
                    status = pollUploadStatus(taskId, *cancelFlag);
                    pollFailures = 0;  // reset on success
                }
                catch(const FileServiceError &pe) {
                    // Transient poll HTTP timeout: device may be busy uploading.
                    // Retry a few times before treating it as a real failure.
                    if(pe.state == OB_SDCARD_DOWNLOAD_FAILED_TIMEOUT &&
                       ++pollFailures <= kMaxPollRetries &&
                       !cancelFlag->load(std::memory_order_relaxed)) {
                        LOG_WARN("HttpFileServiceManager: poll timeout for task {}, retry {}/{}",
                                 taskId, pollFailures, kMaxPollRetries);
                        std::this_thread::sleep_for(std::chrono::milliseconds(kUploadPollIntervalMs));
                        continue;
                    }
                    throw;
                }

                if(cb) {
                    cb(OB_SDCARD_DOWNLOAD_TRANSFERRING,
                       status.fileName.empty() ? name : status.fileName,
                       status.percent, "");
                }

                if(status.statusStr == "done") {
                    releaseSlot();
                    if(cb) cb(OB_SDCARD_DOWNLOAD_DONE, name, 100, status.md5);
                    unregisterOp(cancelFlag);
                    return;
                }
                if(status.statusStr == "cancelled")
                    throw FileServiceError(OB_SDCARD_DOWNLOAD_CANCELLED,
                                           "HTTP file service: upload cancelled on device");
                if(status.statusStr == "failed") {
                    // Map firmware errorCode to specific state.
                    // 1=connect fail, 2=network error, 3=server non-2xx, 4=timeout, 5=local IO, 6=cancel
                    ob_sdcard_download_state fst;
                    switch(status.errorCode) {
                    case 1: case 2: fst = OB_SDCARD_DOWNLOAD_FAILED_NETWORK; break;
                    case 3:         fst = OB_SDCARD_DOWNLOAD_FAILED_SERVER;  break;
                    case 4:         fst = OB_SDCARD_DOWNLOAD_FAILED_TIMEOUT; break;
                    case 6:         fst = OB_SDCARD_DOWNLOAD_CANCELLED;      break;
                    default:        fst = OB_SDCARD_DOWNLOAD_FAILED;         break;
                    }
                    throw FileServiceError(fst,
                                           "HTTP file service: upload failed (errorCode=" +
                                           std::to_string(status.errorCode) + "): " +
                                           status.errorMsg);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(kUploadPollIntervalMs));
            }
        }
        catch(...) {
            releaseSlot();
            if(!taskId.empty()) cancelUploadTask(taskId);
            const auto st = stateFromCurrentException(*cancelFlag);
            if(cb) cb(st, name, -1, "");
            unregisterOp(cancelFlag);
            throw;
        }
        unregisterOp(cancelFlag);  // unreachable but kept for safety
    };

    if(async) {
        // Async: start background thread, swallow exception (already reported via callback).
        std::thread([work]() {
            try { work(); }
            catch(...) { /* exception already delivered to callback */ }
        }).detach();
    }
    else {
        // Sync: block and let exception propagate to the caller.
        work();
    }
}

// ── getFileInfo ────────────────────────────────────────────────────────────

SdcardFileInfo HttpFileServiceManager::getFileInfo(const std::string &path) const {
    quickOpCancelFlag_.store(false, std::memory_order_relaxed);
    CURL       *curl        = curl_easy_init();
    std::string encodedPath = curl ? urlEncode(curl, path) : path;
    if(curl) curl_easy_cleanup(curl);

    const std::string body = httpGet(makeUrl("/api/fs/stat?path=") + encodedPath,
                                     true, kQuickOpTimeoutMs, quickOpCancelFlag_);
    SdcardFileInfo info;
    if(body.empty()) { info.exists = false; return info; }

    Json::Value root;
    std::string errors;
    if(!parseJson(body, root, errors))
        THROW_IO_EXCEPTION("HTTP file service: invalid JSON from stat: " + errors);
    if(root.get("code", -1).asInt() != 0)
        THROW_IO_EXCEPTION("HTTP file service: stat failed (code=" +
                           std::to_string(root.get("code", -1).asInt()) + "): " +
                           root.get("msg", "").asString());

    info.exists = root.get("exists", 0).asInt() != 0;
    info.isDir  = root.get("isDir",  0).asInt() != 0;
    info.size   = static_cast<uint64_t>(root.get("size", 0u).asUInt64());
    return info;
}

// ── listFiles ──────────────────────────────────────────────────────────────

std::vector<SdcardFileEntry> HttpFileServiceManager::listFiles(const std::string &path, bool recursive) const {
    quickOpCancelFlag_.store(false, std::memory_order_relaxed);
    CURL       *curl = curl_easy_init();
    std::string query;
    if(!path.empty()) query = "path=" + (curl ? urlEncode(curl, path) : path);
    if(recursive) { query += (query.empty() ? "" : "&"); query += "recursive=1"; }
    if(curl) curl_easy_cleanup(curl);

    const std::string url     = makeUrl("/api/fs/list") + (query.empty() ? "" : "?" + query);
    const long        timeout = recursive ? kQuickOpTimeoutMs * 2 : kQuickOpTimeoutMs;
    const std::string body    = httpGet(url, false, timeout, quickOpCancelFlag_);

    Json::Value root;
    std::string errors;
    if(!parseJson(body, root, errors))
        THROW_IO_EXCEPTION("HTTP file service: invalid JSON from list: " + errors);
    checkJsonCode(body, "listFiles");

    std::vector<SdcardFileEntry> entries;
    const Json::Value           &arr = root["entries"];
    if(arr.isArray()) {
        entries.reserve(arr.size());
        for(const auto &item : arr) {
            SdcardFileEntry e;
            e.name  = item.get("name",  "").asString();
            e.isDir = item.get("isDir", 0).asInt() != 0;
            e.size  = static_cast<uint64_t>(item.get("size", 0u).asUInt64());
            entries.push_back(std::move(e));
        }
    }
    return entries;
}

// ── deleteFile ─────────────────────────────────────────────────────────────

void HttpFileServiceManager::deleteFile(const std::string &path) const {
    quickOpCancelFlag_.store(false, std::memory_order_relaxed);
    CURL       *curl        = curl_easy_init();
    std::string encodedPath = curl ? urlEncode(curl, path) : path;
    if(curl) curl_easy_cleanup(curl);

    const std::string body = httpPost(makeUrl("/api/fs/delete?path=") + encodedPath + "&confirm=1",
                                      kQuickOpTimeoutMs, quickOpCancelFlag_);
    checkJsonCode(body, "deleteFile");
}

}  // namespace libobsensor
