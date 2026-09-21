// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// EGO SD-card file service sample.
//
// Demonstrates the SD-card file service APIs via ob::DataAcquisitionDevice:
//   getSdcardFileInfo   – query whether a path exists and its metadata
//   getSdcardFileList   – list a directory (flat or recursive)
//   downloadSdcardFile  – download to local file, or push to a remote HTTP URL
//   deleteSdcardFile    – delete a file or directory
//
// Connect via network (enter the device IP at startup).
//
// Key map (triggered from the console):
//   1  – List SD card root (non-recursive)
//   2  – Stat a path (prints exists/type/size; non-existent path prints exists=false, no error)
//   3  – List a specific directory non-recursively
//   4  – List a path recursively
//   5  – Download a single file to local working directory
//   6  – Resume download: download same file again, SDK resumes from existing local bytes
//   7  – Download a directory: list files then download each one (app-level loop)
//   8  – Push file to Tencent COS (auto-generates COS V5 presign URL, credentials hard-coded)
//   U  – Push entire directory to COS: list files, generate per-file URLs, upload all async
//   9  – Directory upload note: requires per-file URLs, see in-sample guidance
//   a  – Async download: start in background (async=true), main loop stays responsive
//   m  – Multi-file concurrent download: list directory, download all files simultaneously
//   c  – Cancel all in-progress operations
//   d  – Delete a file or directory (asks for confirmation)
//   e  – Error path test: try stat with an invalid path (e.g. "../escape") to verify 400 handling
//   h  – Show this help
//   q  – Quit

#include <libobsensor/ObSensor.hpp>

#if defined(_WIN32)
#  include <windows.h>
#endif

#include "utils.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// ── Tencent COS V5 pre-signing ────────────────────────────────────────────
// Implements the same algorithm as cos_presign() in test(1).py.
// Reference: https://cloud.tencent.com/document/product/436/7778
//
// Hard-coded test credentials (same as test(1).py) — replace for production.
static const std::string kCosSecretId   = "AKIDCC5rUxLhz9EAvSmvra62mJcidLq7D6lG";
static const std::string kCosSecretKey  = "PbcQtImFrLpKhrnV2OayDMK6fCJG2e3F";
static const std::string kCosBucketHost = "test-1326643791.cos.ap-guangzhou.myqcloud.com";
static const int         kCosExpireSecs = 600;

// ── SHA-1 (RFC 3174) ──────────────────────────────────────────────────────

static uint32_t sha1Rol(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

static void sha1ProcessBlock(const uint8_t blk[64], uint32_t H[5]) {
    uint32_t W[80];
    for(int i = 0; i < 16; ++i)
        W[i] = (uint32_t(blk[i*4]) << 24) | (uint32_t(blk[i*4+1]) << 16) |
               (uint32_t(blk[i*4+2]) << 8)  | uint32_t(blk[i*4+3]);
    for(int i = 16; i < 80; ++i)
        W[i] = sha1Rol(W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16], 1);

    uint32_t a = H[0], b = H[1], c = H[2], d = H[3], e = H[4];
    for(int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if(i < 20)      { f = (b & c) | (~b & d);          k = 0x5A827999u; }
        else if(i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1u; }
        else if(i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
        else            { f = b ^ c ^ d;                    k = 0xCA62C1D6u; }
        uint32_t t = sha1Rol(a, 5) + f + e + k + W[i];
        e = d; d = c; c = sha1Rol(b, 30); b = a; a = t;
    }
    H[0] += a; H[1] += b; H[2] += c; H[3] += d; H[4] += e;
}

static void sha1Hash(const void *data_, size_t len, uint8_t digest[20]) {
    const uint8_t *data = static_cast<const uint8_t *>(data_);
    uint32_t H[5] = { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u };

    size_t pos = 0;
    while(pos + 64 <= len) { sha1ProcessBlock(data + pos, H); pos += 64; }

    uint8_t pad[128] = {};
    size_t remain = len - pos;
    std::memcpy(pad, data + pos, remain);
    pad[remain] = 0x80;
    size_t padLen = (remain < 56) ? 64 : 128;
    uint64_t bits = uint64_t(len) * 8;
    for(int i = 0; i < 8; ++i) pad[padLen - 1 - i] = uint8_t(bits >> (i * 8));

    sha1ProcessBlock(pad, H);
    if(padLen == 128) sha1ProcessBlock(pad + 64, H);

    for(int i = 0; i < 5; ++i) {
        digest[i*4]   = uint8_t(H[i] >> 24); digest[i*4+1] = uint8_t(H[i] >> 16);
        digest[i*4+2] = uint8_t(H[i] >> 8);  digest[i*4+3] = uint8_t(H[i]);
    }
}

static std::string hexStr(const uint8_t *d, size_t n) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for(size_t i = 0; i < n; ++i) ss << std::setw(2) << int(d[i]);
    return ss.str();
}

// ── HMAC-SHA-1 ────────────────────────────────────────────────────────────
static void hmacSha1(const std::string &key, const std::string &msg, uint8_t out[20]) {
    uint8_t k[64] = {};
    if(key.size() > 64) sha1Hash(key.data(), key.size(), k);
    else std::memcpy(k, key.data(), key.size());

    uint8_t ipad[64], opad[64];
    for(int i = 0; i < 64; ++i) { ipad[i] = k[i] ^ 0x36u; opad[i] = k[i] ^ 0x5Cu; }

    std::vector<uint8_t> inner(64 + msg.size());
    std::memcpy(inner.data(), ipad, 64);
    std::memcpy(inner.data() + 64, msg.data(), msg.size());
    uint8_t innerHash[20];
    sha1Hash(inner.data(), inner.size(), innerHash);

    uint8_t outer_[84];
    std::memcpy(outer_, opad, 64);
    std::memcpy(outer_ + 64, innerHash, 20);
    sha1Hash(outer_, 84, out);
}

// ── Tencent COS V5 presign URL (http://, PUT method) ─────────────────────
// Produces the same URL as cos_presign() in test(1).py with scheme="http".
static std::string cosPresign(const std::string &secretId, const std::string &secretKey,
                               const std::string &host, const std::string &objectKey,
                               int expireSecs = 600) {
    std::time_t now = std::time(nullptr);
    std::string signTime = std::to_string(now - 60) + ";" + std::to_string(now + expireSecs);

    std::string path = (objectKey.empty() || objectKey[0] != '/') ? ("/" + objectKey) : objectKey;
    std::string formatStr = "put\n" + path + "\n\nhost=" + host + "\n";

    uint8_t sha1Fmt[20];
    sha1Hash(formatStr.data(), formatStr.size(), sha1Fmt);
    std::string strToSign = "sha1\n" + signTime + "\n" + hexStr(sha1Fmt, 20) + "\n";

    uint8_t signKeyBytes[20];
    hmacSha1(secretKey, signTime, signKeyBytes);
    uint8_t sigBytes[20];
    hmacSha1(hexStr(signKeyBytes, 20), strToSign, sigBytes);

    std::string q = "q-sign-algorithm=sha1"
                    "&q-ak=" + secretId +
                    "&q-sign-time=" + signTime +
                    "&q-key-time=" + signTime +
                    "&q-header-list=host"
                    "&q-url-param-list="
                    "&q-signature=" + hexStr(sigBytes, 20);

    // Percent-encode object key (keep /-_.~ safe)
    std::string raw = (objectKey.empty() || objectKey[0] == '/') ? objectKey.substr(1) : objectKey;
    std::string encoded;
    for(unsigned char c : raw) {
        if(std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            encoded += c;
        } else {
            std::ostringstream ss;
            ss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << int(c);
            encoded += ss.str();
        }
    }
    return "http://" + host + "/" + encoded + "?" + q;
}

// ── helpers ────────────────────────────────────────────────────────────────

static std::string readLine(const char *prompt) {
#if defined(_WIN32)
    // Restore console echo mode in case waitForKeyPressed() left it disabled.
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD  mode = 0;
    GetConsoleMode(hIn, &mode);
    SetConsoleMode(hIn, mode | ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
#endif
    std::cout << prompt;
    std::string s;
    std::getline(std::cin, s);
    return s;
}

static std::string humanSize(uint64_t bytes) {
    const char *units[] = { "B", "KB", "MB", "GB", "TB" };
    double      v       = static_cast<double>(bytes);
    int         u       = 0;
    while(v >= 1024.0 && u < 4) {
        v /= 1024.0;
        ++u;
    }
    char buf[32];
    if(u == 0) {
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    }
    else {
        std::snprintf(buf, sizeof(buf), "%.1f %s", v, units[u]);
    }
    return buf;
}

// Mutex to serialise console output from concurrent callbacks.
static std::mutex g_printMutex;

// Progress callback (static function, compatible with C callback signature)
static void onProgress(ob_sdcard_download_state state, const char *fileName, int percent, const char *md5_base64, void *) {
    std::lock_guard<std::mutex> lock(g_printMutex);
    switch(state) {
    case OB_SDCARD_DOWNLOAD_TRANSFERRING:
        if(percent < 0) {
            // Size unknown (resume or directory stream) — print once with \r so it overwrites itself
            std::cout << "\r  [" << fileName << "] transferring...    " << std::flush;
        }
        else if(percent == 0) {
            // Start of a new file — use \r so repeated 0% callbacks overwrite each other
            std::cout << "\r  [" << fileName << "] 0%                 " << std::flush;
        }
        else if(percent < 100 && percent % 5 == 0) {
            // Only print at 5% milestones to avoid flooding the console
            std::cout << "\r  [" << fileName << "] " << percent << "%          " << std::flush;
        }
        break;
    case OB_SDCARD_DOWNLOAD_DONE:
        std::cout << "\r  [" << fileName << "] done.";
        if(md5_base64 && *md5_base64)
            std::cout << "  MD5=" << md5_base64;
        std::cout << "                  " << std::endl;
        break;
    case OB_SDCARD_DOWNLOAD_FAILED:
        std::cout << "\r  [" << fileName << "] FAILED.                    " << std::endl;
        break;
    case OB_SDCARD_DOWNLOAD_CANCELLED:
        std::cout << "\r  [" << fileName << "] cancelled.                 " << std::endl;
        break;
    case OB_SDCARD_DOWNLOAD_FAILED_NOT_FOUND:
        std::cout << "\r  [" << fileName << "] FAILED: not found.         " << std::endl;
        break;
    case OB_SDCARD_DOWNLOAD_FAILED_TIMEOUT:
        std::cout << "\r  [" << fileName << "] FAILED: timeout.           " << std::endl;
        break;
    case OB_SDCARD_DOWNLOAD_FAILED_NETWORK:
        std::cout << "\r  [" << fileName << "] FAILED: network error.     " << std::endl;
        break;
    case OB_SDCARD_DOWNLOAD_FAILED_SERVER:
        std::cout << "\r  [" << fileName << "] FAILED: server rejected.   " << std::endl;
        break;
    case OB_SDCARD_DOWNLOAD_FAILED_BUSY:
        std::cout << "\r  [" << fileName << "] FAILED: device busy.       " << std::endl;
        break;
    case OB_SDCARD_DOWNLOAD_FAILED_CAMERA_STREAMING:
        std::cout << "\r  [" << fileName << "] FAILED: camera streaming.  " << std::endl;
        break;
    default:
        break;
    }
}

// ── operations ─────────────────────────────────────────────────────────────

static void doList(ob::DataAcquisitionDevice &dev, const std::string &path, bool recursive) {
    const auto entries = dev.getSdcardFileList(path, recursive);
    if(entries.empty()) {
        std::cout << "  (empty)" << std::endl;
        return;
    }
    std::cout << "  " << entries.size() << " entries:" << std::endl;
    for(size_t i = 0; i < entries.size(); ++i) {
        const auto &e = entries[i];
        std::cout << "  " << std::setw(4) << (i + 1) << "  "
                  << (e.isDir ? "[DIR]  " : "[FILE] ")
                  << std::left << std::setw(52) << e.name;
        if(!e.isDir) {
            std::cout << "  " << humanSize(e.size);
        }
        std::cout << std::endl;
    }
}

static void doStat(ob::DataAcquisitionDevice &dev, const std::string &path) {
    const auto info = dev.getSdcardFileInfo(path);
    if(!info.exists) {
        std::cout << "  Path does not exist: " << path << std::endl;
    }
    else {
        std::cout << "  Path : " << path << std::endl;
        std::cout << "  Type : " << (info.isDir ? "directory" : "file") << std::endl;
        if(!info.isDir) {
            std::cout << "  Size : " << humanSize(info.size) << " (" << info.size << " bytes)" << std::endl;
        }
    }
}

static void doDownloadToLocal(ob::DataAcquisitionDevice &dev, const std::string &devicePath, const std::string &localPath) {
    std::cout << "  Downloading '" << devicePath << "' -> '" << localPath << "' ..." << std::endl;
    dev.downloadSdcardFile(devicePath, OB_SDCARD_DOWNLOAD_TO_LOCAL, localPath, onProgress, nullptr);
}

static void doDownloadToUrl(ob::DataAcquisitionDevice &dev, const std::string &devicePath, const std::string &remoteUrl) {
    std::cout << "  Pushing '" << devicePath << "' -> '" << remoteUrl << "' ..." << std::endl;
    std::cout << "  (blocks until device reports completion)" << std::endl;
    dev.downloadSdcardFile(devicePath, OB_SDCARD_DOWNLOAD_TO_URL, remoteUrl, onProgress, nullptr);
}

static void doDelete(ob::DataAcquisitionDevice &dev, const std::string &path) {
    const std::string prompt  = "  Confirm delete '" + path + "'? [y/N]: ";
    const std::string confirm = readLine(prompt.c_str());
    if(confirm != "y" && confirm != "Y") {
        std::cout << "  Cancelled." << std::endl;
        return;
    }
    dev.deleteSdcardFile(path);
    std::cout << "  Deleted: " << path << std::endl;
}

static void printHelp() {
    std::cout << "\n"
              << "  +-- SD-card File Service ---------------------------------------------------+\n"
              << "  |  1  List SD card root (non-recursive)                                     |\n"
              << "  |  2  Stat a path (exists=false for non-existent, no exception thrown)      |\n"
              << "  |  3  List a specific directory non-recursively                             |\n"
              << "  |  4  List a path recursively                                               |\n"
              << "  |  5  Download single file to local                                         |\n"
              << "  |  6  Resume download: re-download same file, SDK resumes (Range header)    |\n"
              << "  |  7  Download directory: list + download each file (app-level loop)       |\n"
              << "  |  8  Push single file to COS (COS V5 presign, credentials hard-coded)        |\n"
              << "  |  U  Push directory to COS: list + per-file presign + upload all async        |\n"
              << "  |  9  Directory upload note (per-file URLs required, see guidance)          |\n"
              << "  |  a  Async download (async=true): starts in background, main loop free    |\n"
              << "  |  m  Multi-file concurrent download: list dir, download all files async   |\n"
              << "  |  c  Cancel all in-progress operations                                    |\n"
              << "  |  d  Delete a file or directory                                            |\n"
              << "  |  e  Error-path test: stat an invalid path to verify 400 handling          |\n"
              << "  |  h  Show this help                                                        |\n"
              << "  |  q  Quit                                                                  |\n"
              << "  +---------------------------------------------------------------------------+\n"
              << std::endl;
}

// ── main ───────────────────────────────────────────────────────────────────

int main() try {
    ob::Context                 context;
    std::shared_ptr<ob::Device> rawDevice;

    const auto ip = readLine("Enter device IP address: ");
    if(ip.empty()) {
        std::cerr << "No IP address entered." << std::endl;
        return EXIT_FAILURE;
    }
    rawDevice = context.createNetDevice(ip.c_str(), 8090);
    std::cout << "Connected to network device " << ip << std::endl;

    auto device = ob::DataAcquisitionDevice::fromDevice(rawDevice);

    // Quick connectivity check: list root to confirm the file service is up
    std::cout << "\nChecking SD-card file service (listing root) ..." << std::endl;
    {
        const auto root = device->getSdcardFileList("", false);
        std::cout << "SD-card root contains " << root.size() << " top-level entries." << std::endl;
    }

    printHelp();

    while(true) {
        std::cout << "Key> " << std::flush;
        const char key = ob_smpl::waitForKeyPressed();
        std::cout << key << std::endl;

        try {
            switch(key) {
            case '1':
                std::cout << "  Listing SD card root (non-recursive) ..." << std::endl;
                doList(*device, "", false);
                break;

            case '2': {
                const auto path = readLine("  Path to stat (try a non-existent path too): ");
                if(path.empty()) { std::cout << "  (cancelled)" << std::endl; break; }
                doStat(*device, path);
                break;
            }
            case '3': {
                const auto path = readLine("  Directory to list non-recursively (empty=root): ");
                std::cout << "  Listing '" << (path.empty() ? "/" : path) << "' (non-recursive) ..." << std::endl;
                doList(*device, path, false);
                break;
            }
            case '4': {
                const auto path = readLine("  Path to list recursively (empty=root): ");
                std::cout << "  Listing '" << (path.empty() ? "/" : path) << "' recursively ..." << std::endl;
                doList(*device, path, true);
                break;
            }
            case '5': {
                const auto devicePath = readLine("  Device file path (e.g. rec/2026/09/14/a.mp4): ");
                if(devicePath.empty()) { std::cout << "  (cancelled)" << std::endl; break; }
                const auto slash     = devicePath.rfind('/');
                const auto fileName  = (slash == std::string::npos) ? devicePath : devicePath.substr(slash + 1);
                const auto localDir  = readLine("  Local save directory (empty = current dir): ");
                const auto localPath = localDir.empty() ? fileName : localDir + "/" + fileName;
                doDownloadToLocal(*device, devicePath, localPath);
                break;
            }
            case '6': {
                // Resume test: download the same file again; SDK sends Range: bytes=N- to resume.
                const auto devicePath = readLine("  Device file path to resume-download: ");
                if(devicePath.empty()) { std::cout << "  (cancelled)" << std::endl; break; }
                const auto slash     = devicePath.rfind('/');
                const auto localName = (slash == std::string::npos) ? devicePath : devicePath.substr(slash + 1);

                // Check if local file already exists and show resume offset
                std::ifstream existing(localName, std::ios::binary | std::ios::ate);
                if(existing.is_open()) {
                    const auto offset = static_cast<int64_t>(existing.tellg());
                    existing.close();
                    std::cout << "  Local file '" << localName << "' exists (" << offset
                              << " bytes) - SDK will resume from offset " << offset << std::endl;
                }
                else {
                    std::cout << "  No local file found - starting fresh download" << std::endl;
                }
                doDownloadToLocal(*device, devicePath, localName);
                break;
            }
            case '7': {
                // Directory download: list all files then download each one individually.
                // This demonstrates how the application should handle directory download.
                const auto dirPath = readLine("  Device directory path to download: ");
                if(dirPath.empty()) { std::cout << "  (cancelled)" << std::endl; break; }
                const auto localDir = readLine("  Local save directory (empty = current dir): ");
                const auto outBase  = localDir.empty() ? std::string(".") : localDir;

                std::cout << "  Listing '" << dirPath << "' recursively ..." << std::endl;
                const auto entries = device->getSdcardFileList(dirPath, true);

                std::vector<ob_device_sdcard_file_entry> files;
                for(const auto &e : entries) {
                    if(!e.isDir) files.push_back(e);
                }
                if(files.empty()) { std::cout << "  No files found." << std::endl; break; }
                std::cout << "  Found " << files.size() << " files, downloading..." << std::endl;

                int done = 0;
                for(const auto &f : files) {
                    const std::string localPath = outBase + "/" + f.name;
                    // Create parent directories for nested files
                    const auto slash = localPath.find_last_of("/\\");
                    if(slash != std::string::npos) {
                        std::string dirToCreate = localPath.substr(0, slash);
#if defined(_WIN32)
                        std::string cmd = "if not exist \"" + dirToCreate + "\" mkdir \"" + dirToCreate + "\"";
                        std::system(cmd.c_str());
#else
                        std::system(("mkdir -p \"" + dirToCreate + "\"").c_str());
#endif
                    }
                    std::cout << "  [" << (++done) << "/" << files.size() << "] " << f.name << std::endl;
                    try {
                        doDownloadToLocal(*device, f.name, localPath);
                    }
                    catch(const ob::Error &e) {
                        std::cerr << "    FAILED: " << e.what() << std::endl;
                    }
                }
                std::cout << "  Directory download complete: " << done << "/" << files.size() << " files." << std::endl;
                break;
            }
            case '8': {
                // Push a single file to Tencent COS using auto-generated COS V5 presign URL.
                // Credentials: kCosSecretId / kCosSecretKey / kCosBucketHost (hard-coded above).
                const auto devicePath = readLine("  Device file path to push to COS: ");
                if(devicePath.empty()) { std::cout << "  (cancelled)" << std::endl; break; }
                const std::string url  = cosPresign(kCosSecretId, kCosSecretKey,
                                                     kCosBucketHost, devicePath, kCosExpireSecs);
                std::cout << "  COS V5 presign URL (valid " << kCosExpireSecs << "s):\n"
                          << "  " << url << std::endl;
                doDownloadToUrl(*device, devicePath, url);
                break;
            }
            case 'U': case 'u': {
                // Push an entire directory to Tencent COS.
                // The SDK automatically limits to 2 concurrent uploads (firmware limit).
                // Excess requests wait in the SDK until a slot is free — no 503 errors.
                const auto dirPath = readLine("  Device directory path to push to COS: ");
                if(dirPath.empty()) { std::cout << "  (cancelled)" << std::endl; break; }

                std::cout << "  Listing '" << dirPath << "' recursively ..." << std::endl;
                const auto entries = device->getSdcardFileList(dirPath, true);

                std::vector<ob_device_sdcard_file_entry> files;
                for(const auto &e : entries) {
                    if(!e.isDir) files.push_back(e);
                }
                if(files.empty()) { std::cout << "  No files found." << std::endl; break; }
                std::cout << "  Found " << files.size()
                          << " files, starting async uploads to COS ..." << std::endl;
                std::cout << "  (SDK queues to max 2 concurrent; press c to cancel all)" << std::endl;

                for(const auto &f : files) {
                    const std::string url = cosPresign(kCosSecretId, kCosSecretKey,
                                                       kCosBucketHost, f.name, kCosExpireSecs);
                    device->downloadSdcardFile(f.name, OB_SDCARD_DOWNLOAD_TO_URL, url,
                                               onProgress, nullptr, true);  // async=true
                }
                break;
            }

            case '9': {
                // Directory upload: the new firmware requires per-file pre-signed URLs
                // (POST with JSON body {"files":[{file,url},...]}). That API is not yet
                // implemented in this sample — use key 8 for single-file upload instead.
                std::cout << "  Directory upload to URL requires per-file pre-signed URLs.\n"
                          << "  Use key 8 to upload a single file, or call ob_device_get_sdcard_file_list\n"
                          << "  to enumerate files, generate URLs externally, then upload each via key 8."
                          << std::endl;
                break;
            }
            // ── async / concurrent tests ──────────────────────────────
            case 'a': case 'A': {
                // Async single-file download: SDK starts a background thread and
                // returns immediately. The main loop stays responsive — press other
                // keys while the download is happening. Press 'c' to cancel.
                const auto devicePath = readLine("  Device file path (async=true): ");
                if(devicePath.empty()) { std::cout << "  (cancelled)" << std::endl; break; }
                const auto localDir  = readLine("  Local save directory (empty = current dir): ");
                const auto slash     = devicePath.rfind('/');
                const auto fileName  = (slash == std::string::npos) ? devicePath : devicePath.substr(slash + 1);
                const auto localPath = localDir.empty() ? fileName : localDir + "/" + fileName;
                std::cout << "  Starting async download '" << devicePath << "' -> '" << localPath << "' ..." << std::endl;
                std::cout << "  (running in background — press other keys while it downloads; press c to cancel)" << std::endl;
                device->downloadSdcardFile(devicePath, OB_SDCARD_DOWNLOAD_TO_LOCAL, localPath, onProgress, nullptr, true);
                break;
            }
            case 'm': case 'M': {
                // Multi-file concurrent download: list a directory, then start an
                // async download for EVERY file simultaneously. Each file has its own
                // background thread and independent progress/cancel.
                const auto dirPath = readLine("  Device directory to download concurrently: ");
                if(dirPath.empty()) { std::cout << "  (cancelled)" << std::endl; break; }
                const auto localDir = readLine("  Local save directory (empty = current dir): ");
                const auto outBase  = localDir.empty() ? std::string(".") : localDir;

                std::cout << "  Listing '" << dirPath << "' recursively ..." << std::endl;
                const auto entries = device->getSdcardFileList(dirPath, true);

                std::vector<ob_device_sdcard_file_entry> files;
                for(const auto &e : entries) {
                    if(!e.isDir) files.push_back(e);
                }
                if(files.empty()) { std::cout << "  No files found." << std::endl; break; }

                std::cout << "  Starting " << files.size() << " concurrent async downloads ..." << std::endl;
                std::cout << "  (all running in background simultaneously; press c to cancel all)" << std::endl;

                for(const auto &f : files) {
                    const std::string localPath = outBase + "/" + f.name;
                    // Create parent directories
                    const auto lastSlash = localPath.find_last_of("/\\");
                    if(lastSlash != std::string::npos) {
                        std::string dirToCreate = localPath.substr(0, lastSlash);
#if defined(_WIN32)
                        std::string cmd = "if not exist \"" + dirToCreate + "\" mkdir \"" + dirToCreate + "\"";
                        std::system(cmd.c_str());
#else
                        std::system(("mkdir -p \"" + dirToCreate + "\"").c_str());
#endif
                    }
                    device->downloadSdcardFile(f.name, OB_SDCARD_DOWNLOAD_TO_LOCAL, localPath,
                                               onProgress, nullptr, true);  // async=true
                }
                break;
            }
            case 'c': case 'C': {
                // Cancel all in-progress download/upload operations.
                std::cout << "  Cancelling all in-progress operations ..." << std::endl;
                device->cancelSdcardFileOperation();
                std::cout << "  Cancel signal sent." << std::endl;
                break;
            }

            case 'd': case 'D': {
                const auto path = readLine("  Path to delete: ");
                if(path.empty()) { std::cout << "  (cancelled)" << std::endl; break; }
                doDelete(*device, path);
                break;
            }
            case 'e': case 'E': {
                // Error-path test: try an invalid path; firmware should return HTTP 400.
                const auto badPath = readLine("  Invalid path to test (e.g. ../escape or /absolute): ");
                if(badPath.empty()) { std::cout << "  (cancelled)" << std::endl; break; }
                std::cout << "  Stat-ing invalid path '" << badPath << "' — expect SDK error (400) ..." << std::endl;
                doStat(*device, badPath);  // will throw ob::Error on 400, caught below
                break;
            }
            case 'h': case 'H':
                printHelp();
                break;
            case 'q': case 'Q':
                std::cout << "Quitting." << std::endl;
                return EXIT_SUCCESS;
            default:
                std::cout << "  Unknown key '" << key << "'. Press h for help." << std::endl;
                break;
            }
        }
        catch(const ob::Error &e) {
            std::cerr << "  [error] " << e.what() << std::endl;
        }
    }
}
catch(const ob::Error &e) {
    std::cerr << "SDK error: " << e.what() << " (status=" << e.getStatus() << ")" << std::endl;
    return EXIT_FAILURE;
}
catch(const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
}
