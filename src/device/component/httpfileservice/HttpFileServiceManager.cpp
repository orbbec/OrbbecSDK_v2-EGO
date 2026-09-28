// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "HttpFileServiceManager.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"

#include <curl/curl.h>
#include <json/json.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <limits>
#include <sstream>
#include <thread>
#include <utility>

namespace libobsensor {

// ── libcurl helpers ────────────────────────────────────────────────────────

namespace {

size_t writeToString(char *ptr, size_t size, size_t nmemb, void *userdata) {
    static_cast<std::string *>(userdata)->append(ptr, size * nmemb);
    return size * nmemb;
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

struct ChunkResponseHeaders {
    uint32_t    index = static_cast<uint32_t>(~0u);
    uint32_t    count = 0;
    uint64_t    size = 0;
    uint64_t    fileSize = 0;
    std::string md5;
};

size_t chunkHeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata) {
    const std::string line(buffer, size * nitems);
    auto *headers = static_cast<ChunkResponseHeaders *>(userdata);
    const size_t colon = line.find(':');
    if(colon == std::string::npos) return line.size();

    std::string key = line.substr(0, colon);
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    const size_t start = line.find_first_not_of(" \t", colon + 1);
    const size_t end   = line.find_last_not_of(" \t\r\n");
    const std::string value = start == std::string::npos ? "" : line.substr(start, end - start + 1);
    if(key == "x-chunk-index") headers->index = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
    else if(key == "x-chunk-size") headers->size = std::strtoull(value.c_str(), nullptr, 10);
    else if(key == "x-chunk-count") headers->count = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
    else if(key == "x-file-size") headers->fileSize = std::strtoull(value.c_str(), nullptr, 10);
    else if(key == "x-chunk-md5") headers->md5 = value;
    return line.size();
}

// ── FileServiceError: typed exception that carries a specific download-state ──
// Thrown by throwCurlError / checkHttpCode so catch blocks can set the exact
// ob_sdcard_download_state without fragile string matching.
struct FileServiceError : public std::exception {
    ob_sdcard_download_state state;
    long                     httpStatus;
    std::string              msg;
    FileServiceError(ob_sdcard_download_state s, std::string m, long status = 0)
        : state(s), httpStatus(status), msg(std::move(m)) {}
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
                               "HTTP file service: path not found (" + msg + ")", httpCode);
    if(httpCode == 503)
        throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_BUSY,
                               "HTTP file service: upload task slots full (" + msg + ")", httpCode);
    if(httpCode == 409)
        throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_CAMERA_STREAMING,
                               "HTTP file service: camera streaming, operation not allowed (" + msg + ")", httpCode);
    if(httpCode >= 500)
        throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED_SERVER,
                               "HTTP file service: " + url + " server error " + msg, httpCode);
    throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED,
                           "HTTP file service: " + url + " returned " + msg, httpCode);
}

constexpr long kConnectTimeoutMs         = 5000;
constexpr long kDownloadConnectTimeoutMs = 3000;
constexpr long kQuickOpTimeoutMs     = 30000;
constexpr long kUploadPollTimeoutMs  = 30000;   // 30 s — device may be slow to respond while uploading
constexpr long kLowSpeedLimitBps     = 1;
constexpr long kLowSpeedTimeSec      = 30;    // fail quickly when transfer makes no progress
constexpr int  kFileServicePort      = 8080;
constexpr int  kUploadPollIntervalMs = 500;
constexpr int  kMaxDownloadRetries   = 1;     // retry a failed chunk once
constexpr size_t kMaxConcurrentChunkDownloads = 3;

// Minimal MD5 implementation used only to verify the firmware's per-chunk
// checksum. Keeping it here avoids adding a public SDK dependency.
class Md5 {
public:
    Md5() : bitCount_(0), bufferSize_(0) {
        state_[0] = 0x67452301; state_[1] = 0xefcdab89;
        state_[2] = 0x98badcfe; state_[3] = 0x10325476;
    }

    void update(const unsigned char *data, size_t len) {
        bitCount_ += static_cast<uint64_t>(len) * 8;
        if(bufferSize_) {
            const size_t take = (std::min)(len, 64 - bufferSize_);
            std::memcpy(buffer_.data() + bufferSize_, data, take);
            bufferSize_ += take; data += take; len -= take;
            if(bufferSize_ == 64) { transform(buffer_.data()); bufferSize_ = 0; }
        }
        while(len >= 64) { transform(data); data += 64; len -= 64; }
        if(len) { std::memcpy(buffer_.data(), data, len); bufferSize_ = len; }
    }

    std::array<unsigned char, 16> finish() {
        const uint64_t originalBits = bitCount_;
        static const unsigned char padding[64] = { 0x80 };
        const size_t pad = bufferSize_ < 56 ? 56 - bufferSize_ : 120 - bufferSize_;
        update(padding, pad);
        unsigned char length[8];
        for(int i = 0; i < 8; ++i) length[i] = static_cast<unsigned char>(originalBits >> (8 * i));
        update(length, sizeof(length));
        std::array<unsigned char, 16> result{};
        for(int i = 0; i < 4; ++i)
            for(int j = 0; j < 4; ++j) result[4 * i + j] = static_cast<unsigned char>(state_[i] >> (8 * j));
        return result;
    }

private:
    static uint32_t rotateLeft(uint32_t value, uint32_t amount) { return (value << amount) | (value >> (32 - amount)); }
    static uint32_t f(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
    static uint32_t g(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
    static uint32_t h(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
    static uint32_t i(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }
    void step(uint32_t (*fn)(uint32_t, uint32_t, uint32_t), uint32_t &a, uint32_t b, uint32_t c, uint32_t d,
              uint32_t x, uint32_t t, uint32_t s) { a = b + rotateLeft(a + fn(b, c, d) + x + t, s); }
    void transform(const unsigned char block[64]) {
        static const uint32_t shifts[64] = { 7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22, 5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20, 4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23, 6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21 };
        static const uint32_t table[64] = { 0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391 };
        uint32_t words[16];
        for(int k = 0; k < 16; ++k) words[k] = static_cast<uint32_t>(block[4*k]) | (static_cast<uint32_t>(block[4*k+1]) << 8) | (static_cast<uint32_t>(block[4*k+2]) << 16) | (static_cast<uint32_t>(block[4*k+3]) << 24);
        uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        for(int k = 0; k < 64; ++k) {
            uint32_t (*fn)(uint32_t,uint32_t,uint32_t) = k < 16 ? f : k < 32 ? g : k < 48 ? h : i;
            const uint32_t index = k < 16 ? k : k < 32 ? (5*k + 1) % 16 : k < 48 ? (3*k + 5) % 16 : (7*k) % 16;
            step(fn, a, b, c, d, words[index], table[k], shifts[k]);
            const uint32_t oldD = d; d = c; c = b; b = a; a = oldD;
        }
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    }
    uint32_t state_[4]; uint64_t bitCount_; std::array<unsigned char, 64> buffer_; size_t bufferSize_;
};

std::string base64Encode(const std::array<unsigned char, 16> &bytes) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out; out.reserve(24);
    for(size_t i = 0; i < bytes.size(); i += 3) {
        const uint32_t value = (static_cast<uint32_t>(bytes[i]) << 16) |
                               (static_cast<uint32_t>(i + 1 < bytes.size() ? bytes[i + 1] : 0) << 8) |
                               static_cast<uint32_t>(i + 2 < bytes.size() ? bytes[i + 2] : 0);
        out.push_back(alphabet[(value >> 18) & 0x3f]); out.push_back(alphabet[(value >> 12) & 0x3f]);
        out.push_back(i + 1 < bytes.size() ? alphabet[(value >> 6) & 0x3f] : '=');
        out.push_back(i + 2 < bytes.size() ? alphabet[value & 0x3f] : '=');
    }
    return out;
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

// ── Chunk download ────────────────────────────────────────────────────────

HttpFileServiceManager::ChunkManifest
HttpFileServiceManager::fetchChunkManifest(const std::string &devicePath,
                                           const std::atomic<bool> &cancelFlag) const {
    CURL *curl = curl_easy_init();
    const std::string path = curl ? urlEncode(curl, devicePath) : devicePath;
    if(curl) curl_easy_cleanup(curl);
    const std::string body = httpGet(makeUrl("/api/fs/chunk/manifest?path=") + path,
                                     false, kQuickOpTimeoutMs, cancelFlag);
    Json::Value root; std::string errors;
    if(!parseJson(body, root, errors)) THROW_IO_EXCEPTION("HTTP file service: invalid chunk manifest: " + errors);
    if(root.get("code", -1).asInt() != 0) THROW_IO_EXCEPTION("HTTP file service: chunk manifest failed: " + root.get("msg", "").asString());
    ChunkManifest manifest;
    manifest.fileSize = root.get("fileSize", 0).asUInt64();
    manifest.chunkSize = root.get("chunkSize", 0).asUInt64();
    const Json::Value &chunks = root["chunks"];
    if(!chunks.isArray() || chunks.size() != root.get("chunkCount", 0).asUInt())
        THROW_IO_EXCEPTION("HTTP file service: invalid chunk manifest entries");
    manifest.chunkSizes.resize(chunks.size());
    uint64_t total = 0;
    for(Json::ArrayIndex i = 0; i < chunks.size(); ++i) {
        if(chunks[i].get("index", Json::UInt(static_cast<uint32_t>(~0u))).asUInt() != i)
            THROW_IO_EXCEPTION("HTTP file service: non-contiguous chunk manifest");
        manifest.chunkSizes[i] = chunks[i].get("size", 0).asUInt64();
        const bool isLastChunk = i + 1 == chunks.size();
        if(manifest.chunkSizes[i] == 0 || manifest.chunkSizes[i] > manifest.chunkSize ||
           (!isLastChunk && manifest.chunkSizes[i] != manifest.chunkSize))
            THROW_IO_EXCEPTION("HTTP file service: invalid chunk size in manifest");
        if(manifest.chunkSizes[i] > (std::numeric_limits<uint64_t>::max)() - total)
            THROW_IO_EXCEPTION("HTTP file service: chunk manifest size overflow");
        total += manifest.chunkSizes[i];
    }
    if(total != manifest.fileSize || (manifest.fileSize == 0 && !manifest.chunkSizes.empty()) ||
       (!manifest.chunkSizes.empty() && manifest.chunkSize == 0) ||
       manifest.fileSize > static_cast<uint64_t>((std::numeric_limits<std::streamoff>::max)()))
        THROW_IO_EXCEPTION("HTTP file service: inconsistent chunk manifest");
    return manifest;
}

HttpFileServiceManager::DownloadedChunk
HttpFileServiceManager::fetchChunk(const std::string &devicePath, uint32_t index,
                                   const std::atomic<bool> &cancelFlag) const {
    ChunkResponseHeaders header;
    CURL *curl = curl_easy_init();
    if(!curl) THROW_IO_EXCEPTION("HttpFileServiceManager: curl_easy_init() failed");
    const std::string path = urlEncode(curl, devicePath);
    const std::string url = makeUrl("/api/fs/chunk?path=") + path + "&index=" + std::to_string(index);
    std::string body; ProgressCtx ctx{ nullptr, &cancelFlag };
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str()); curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString); curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, chunkHeaderCallback); curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, kDownloadConnectTimeoutMs); curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, kLowSpeedLimitBps); curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, kLowSpeedTimeSec); attachCancelAndProgress(curl, &ctx);
    const CURLcode res = curl_easy_perform(curl); long code = 0; curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code); curl_easy_cleanup(curl);
    if(res != CURLE_OK) {
        throwCurlError(res, url);
    }
    checkHttpCode(code, body, url);
    DownloadedChunk result; result.index = header.index; result.size = header.size; result.chunkCount = header.count; result.fileSize = header.fileSize; result.md5Base64 = header.md5; result.data = std::move(body);
    return result;
}

std::vector<bool> HttpFileServiceManager::loadChunkProgress(const std::string &progressPath, const ChunkManifest &manifest) const {
    std::vector<bool> verified(manifest.chunkSizes.size(), false); std::ifstream in(progressPath);
    if(!in.is_open()) return verified;
    Json::Value root; std::string errors;
    if(!Json::parseFromStream(Json::CharReaderBuilder(), in, &root, &errors) ||
       root.get("fileSize", 0).asUInt64() != manifest.fileSize || root.get("chunkSize", 0).asUInt64() != manifest.chunkSize ||
       root.get("chunkCount", 0).asUInt() != manifest.chunkSizes.size()) return std::vector<bool>(manifest.chunkSizes.size(), false);
    for(const auto &index : root["verified"]) if(index.asUInt() < verified.size()) verified[index.asUInt()] = true;
    return verified;
}

void HttpFileServiceManager::saveChunkProgress(const std::string &progressPath, const ChunkManifest &manifest, const std::vector<bool> &verified) const {
    Json::Value root; root["fileSize"] = Json::UInt64(manifest.fileSize); root["chunkSize"] = Json::UInt64(manifest.chunkSize); root["chunkCount"] = Json::UInt(static_cast<unsigned>(verified.size()));
    for(size_t i = 0; i < verified.size(); ++i) if(verified[i]) root["verified"].append(Json::UInt(static_cast<unsigned>(i)));
    const std::string temporary = progressPath + ".tmp"; std::ofstream out(temporary, std::ios::trunc);
    if(!out.is_open()) THROW_IO_EXCEPTION("HTTP file service: cannot write download progress: " + progressPath);
    Json::StreamWriterBuilder writer; writer["indentation"] = ""; out << Json::writeString(writer, root); out.close();
    std::remove(progressPath.c_str()); if(std::rename(temporary.c_str(), progressPath.c_str()) != 0) THROW_IO_EXCEPTION("HTTP file service: cannot commit download progress: " + progressPath);
}

void HttpFileServiceManager::downloadSingleFileToLocal(const std::string &devicePath, const std::string &localPath,
                                                        SdcardDownloadProgressCallback cb, const std::atomic<bool> &cancelFlag) const {
    const ChunkManifest manifest = fetchChunkManifest(devicePath, cancelFlag);
    const std::string name = devicePath.substr(devicePath.find_last_of('/') + 1); const std::string progressPath = localPath + ".obchunks";
    if(manifest.chunkSizes.empty()) {
        LOG_INFO("HttpFileServiceManager: chunk download file='{}', totalChunks=0, pendingChunks=0, maxConcurrentChunks={}",
                 devicePath, kMaxConcurrentChunkDownloads);
        std::ofstream empty(localPath, std::ios::binary | std::ios::trunc);
        if(!empty) THROW_IO_EXCEPTION("HTTP file service: cannot create local file: " + localPath);
        std::remove(progressPath.c_str());
        if(cb) cb(OB_SDCARD_DOWNLOAD_DONE, name, 100, "");
        return;
    }
    std::ifstream existing(localPath, std::ios::binary | std::ios::ate);
    const bool localFileExists = existing.is_open();
    const uint64_t localFileSize = localFileExists ? static_cast<uint64_t>(existing.tellg()) : 0;
    existing.close();
    std::vector<bool> verified = localFileExists ? loadChunkProgress(progressPath, manifest)
                                                 : std::vector<bool>(manifest.chunkSizes.size(), false);
    if(!localFileExists || localFileSize != manifest.fileSize) {
        // A sidecar is valid only together with a preallocated file of exactly
        // the manifest size. Reset both when an old/incomplete file is found.
        std::fill(verified.begin(), verified.end(), false);
        std::ofstream pre(localPath, std::ios::binary | std::ios::trunc);
        if(!pre) THROW_IO_EXCEPTION("HTTP file service: cannot create local file: " + localPath);
        pre.seekp(static_cast<std::streamoff>(manifest.fileSize - 1));
        pre.put('\0');
        if(!pre.good()) THROW_IO_EXCEPTION("HTTP file service: cannot preallocate local file: " + localPath);
    }
    uint64_t completed = 0; for(size_t i = 0; i < verified.size(); ++i) if(verified[i]) completed += manifest.chunkSizes[i];
    if(cb) cb(OB_SDCARD_DOWNLOAD_TRANSFERRING, name, manifest.fileSize ? static_cast<int>((completed * 100) / manifest.fileSize) : 0, "");
    std::vector<uint32_t> pendingIndexes;
    pendingIndexes.reserve(manifest.chunkSizes.size());
    for(uint32_t i = 0; i < manifest.chunkSizes.size(); ++i) {
        if(!verified[i]) pendingIndexes.push_back(i);
    }
    LOG_INFO("HttpFileServiceManager: chunk download file='{}', totalChunks={}, pendingChunks={}, maxConcurrentChunks={}",
             devicePath, manifest.chunkSizes.size(), pendingIndexes.size(), kMaxConcurrentChunkDownloads);

    for(size_t next = 0; next < pendingIndexes.size();) {
        const size_t batchSize = (std::min)(kMaxConcurrentChunkDownloads, pendingIndexes.size() - next);
        std::array<DownloadedChunk, kMaxConcurrentChunkDownloads> chunks;
        std::array<std::exception_ptr, kMaxConcurrentChunkDownloads> errors;
        std::array<uint32_t, kMaxConcurrentChunkDownloads> indexes{};
        std::vector<std::thread> workers;
        workers.reserve(batchSize);

        try {
            for(size_t slot = 0; slot < batchSize; ++slot) {
                indexes[slot] = pendingIndexes[next + slot];
                workers.emplace_back([this, &manifest, &devicePath, &cancelFlag, &chunks, &errors, &indexes, slot]() {
                try {
                    const uint32_t index = indexes[slot];
                    for(int attempt = 0; attempt <= kMaxDownloadRetries; ++attempt) {
                        if(cancelFlag.load(std::memory_order_relaxed))
                            throw FileServiceError(OB_SDCARD_DOWNLOAD_CANCELLED, "HTTP file service: operation cancelled");
                        try {
                            DownloadedChunk chunk = fetchChunk(devicePath, index, cancelFlag);
                            Md5 md5;
                            md5.update(reinterpret_cast<const unsigned char *>(chunk.data.data()), chunk.data.size());
                            const bool accepted = chunk.index == index &&
                                                  chunk.chunkCount == manifest.chunkSizes.size() &&
                                                  chunk.fileSize == manifest.fileSize &&
                                                  chunk.size == manifest.chunkSizes[index] &&
                                                  chunk.data.size() == manifest.chunkSizes[index] &&
                                                  !chunk.md5Base64.empty() &&
                                                  base64Encode(md5.finish()) == chunk.md5Base64;
                            if(accepted) {
                                chunks[slot] = std::move(chunk);
                                return;
                            }
                            LOG_WARN("HttpFileServiceManager: invalid chunk {} for '{}', retry {}/{}",
                                     index, devicePath, attempt + 1, kMaxDownloadRetries + 1);
                        }
                        catch(const FileServiceError &e) {
                            if((e.state != OB_SDCARD_DOWNLOAD_FAILED_TIMEOUT &&
                                e.state != OB_SDCARD_DOWNLOAD_FAILED_NETWORK) ||
                               attempt == kMaxDownloadRetries)
                                throw;
                        }
                    }
                    throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED,
                                           "HTTP file service: chunk checksum validation failed");
                }
                catch(...) {
                    errors[slot] = std::current_exception();
                }
                });
            }
        }
        catch(...) {
            for(auto &worker : workers) {
                if(worker.joinable()) worker.join();
            }
            throw;
        }
        for(auto &worker : workers) worker.join();

        for(size_t slot = 0; slot < batchSize; ++slot) {
            if(!errors[slot]) continue;
            try {
                std::rethrow_exception(errors[slot]);
            }
            catch(const FileServiceError &e) {
                if(e.httpStatus == 416) {
                    // The file changed after its manifest was read. Discard the
                    // verified bitmap and restart from a fresh manifest.
                    std::remove(progressPath.c_str());
                    return downloadSingleFileToLocal(devicePath, localPath, cb, cancelFlag);
                }
                throw;
            }
        }
        if(cancelFlag.load(std::memory_order_relaxed))
            throw FileServiceError(OB_SDCARD_DOWNLOAD_CANCELLED, "HTTP file service: operation cancelled");

        // Network reads are concurrent; writes and sidecar updates stay serial
        // so a persisted verified index always corresponds to durable file data.
        for(size_t slot = 0; slot < batchSize; ++slot) {
            if(cancelFlag.load(std::memory_order_relaxed))
                throw FileServiceError(OB_SDCARD_DOWNLOAD_CANCELLED, "HTTP file service: operation cancelled");
            const uint32_t index = indexes[slot];
            const DownloadedChunk &chunk = chunks[slot];
            std::fstream output(localPath, std::ios::binary | std::ios::in | std::ios::out);
            if(!output) THROW_IO_EXCEPTION("HTTP file service: cannot write local file: " + localPath);
            output.seekp(static_cast<std::streamoff>(static_cast<uint64_t>(index) * manifest.chunkSize));
            output.write(chunk.data.data(), static_cast<std::streamsize>(chunk.data.size()));
            if(!output.good()) THROW_IO_EXCEPTION("HTTP file service: failed writing chunk to local file");
            output.close();

            verified[index] = true;
            completed += chunk.data.size();
            saveChunkProgress(progressPath, manifest, verified);
            if(cb) cb(OB_SDCARD_DOWNLOAD_TRANSFERRING, name,
                      static_cast<int>((completed * 100) / manifest.fileSize), "");
        }
        next += batchSize;
    }
    if(completed != manifest.fileSize) throw FileServiceError(OB_SDCARD_DOWNLOAD_FAILED, "HTTP file service: completed size does not match manifest");
    std::ifstream result(localPath, std::ios::binary | std::ios::ate); if(!result || static_cast<uint64_t>(result.tellg()) != manifest.fileSize) THROW_IO_EXCEPTION("HTTP file service: local file size does not match manifest");
    std::remove(progressPath.c_str()); if(cb) cb(OB_SDCARD_DOWNLOAD_DONE, name, 100, "");
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

    const std::string postUrl = makeUrl("/api/fs/upload?path=") + encodedPath + "&url=" + encodedUrl + "&method=put";
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
