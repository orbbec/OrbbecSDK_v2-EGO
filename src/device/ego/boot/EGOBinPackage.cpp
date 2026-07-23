// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#include "EGOBinPackage.hpp"
#include "EGOMd5.hpp"
#include "exception/ObException.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
namespace libobsensor {
namespace {
std::string trim(std::string s) {
    auto n = [](unsigned char c) { return !std::isspace(c); };
    auto b = std::find_if(s.begin(), s.end(), n);
    auto e = std::find_if(s.rbegin(), s.rend(), n).base();
    if(b < e) {
        return std::string(b, e);
    }
    return std::string();
}
uint32_t hex32(const std::string &s) {
    std::string value = s;
    if(value.size() >= 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        value = value.substr(2);
    }

    // SigmaStar-generated EGO BIN scripts use fields such as 0x21000000.
    // The prefix is not part of the 32-bit hexadecimal value length.
    if(value.empty() || value.size() > 8) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid EGO BIN hexadecimal field");
    }
    size_t        used = 0;
    unsigned long v    = 0;
    try {
        v = std::stoul(value, &used, 16);
    }
    catch(...) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid EGO BIN hexadecimal field");
    }
    if(used != value.size() || v > 0xffffffffUL) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid EGO BIN hexadecimal field");
    }
    return static_cast<uint32_t>(v);
}
uint8_t nibble(uint8_t c) {
    if(c >= '0' && c <= '9') {
        return c - '0';
    }
    if(c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if(c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    THROW_INVALID_PARAM_EXCEPTION("Invalid EGO BIN MD5 trailer");
}
void range(const EGOBinSegment &s, uint64_t limit, const char *name) {
    if(!s.size || uint64_t(s.offset) + s.size > limit) {
        THROW_INVALID_PARAM_EXCEPTION(std::string("Invalid EGO BIN segment: ") + name);
    }
}
}  // namespace
EGOBinPackage EGOBinParser::parseAndVerify(const std::vector<uint8_t> &fw) {
    if(fw.size() < 34) {
        THROW_INVALID_PARAM_EXCEPTION("EGO BIN is too small");
    }
    EGOBinPackage p;
    p.payloadSize                  = fw.size() - 32;
    auto                    actual = egoMd5(fw.data(), static_cast<size_t>(p.payloadSize));
    std::array<uint8_t, 16> expected{};
    for(size_t i = 0; i < 16; ++i) {
        expected[i] = uint8_t((nibble(fw[p.payloadSize + 2 * i]) << 4) | nibble(fw[p.payloadSize + 2 * i + 1]));
    }
    if(actual != expected) {
        THROW_INVALID_PARAM_EXCEPTION("EGO BIN MD5 verification failed");
    }
    size_t pos = 0;
    bool   end = false, hasUpdater = false, hasUboot = false;
    while(pos < p.payloadSize) {
        size_t lineEnd = pos;
        while(lineEnd < p.payloadSize && fw[lineEnd] != '\n' && fw[lineEnd] != '\r') {
            ++lineEnd;
        }
        std::string line(reinterpret_cast<const char *>(fw.data() + pos), lineEnd - pos);
        while(lineEnd < p.payloadSize && (fw[lineEnd] == '\n' || fw[lineEnd] == '\r')) {
            ++lineEnd;
        }
        pos  = lineEnd;
        line = trim(line);
        if(line.empty()) {
            continue;
        }
        if(line.size() >= 2 && line[0] == '#' && line[1] == '@') {
            THROW_INVALID_PARAM_EXCEPTION("EGO BIN requires an external custom script, which is unsupported");
        }
        if(line[0] == '#') {
            continue;
        }
        if(line[0] == '%') {
            end = true;
            break;
        }
        std::istringstream in(line);
        std::string        op, a, b, c;
        in >> op;
        if(op == "SCRIPT") {
            continue;
        }
        if(op == "USB_UPDATER") {
            if(!(in >> a >> b) || (in >> c) || hasUpdater) {
                THROW_INVALID_PARAM_EXCEPTION("Invalid USB_UPDATER directive");
            }
            p.usbUpdater.offset = hex32(a);
            p.usbUpdater.size   = hex32(b);
            hasUpdater   = true;
            continue;
        }
        if(op == "U-BOOT") {
            if(!(in >> a >> b) || (in >> c) || hasUboot) {
                THROW_INVALID_PARAM_EXCEPTION("Invalid U-BOOT directive");
            }
            p.uboot.offset = hex32(a);
            p.uboot.size   = hex32(b);
            hasUboot = true;
            continue;
        }
        EGOUpgradeCommand cmd;
        cmd.text = line;
        if(op == "usbload") {
            std::string extra;
            if(!(in >> a >> b >> c) || (in >> extra)) {
                THROW_INVALID_PARAM_EXCEPTION("Invalid usbload directive");
            }
            cmd.type    = EGOUpgradeCommand::UsbLoad;
            cmd.address = hex32(a);
            cmd.offset  = hex32(b);
            cmd.size    = hex32(c);
            EGOBinSegment segment;
            segment.offset = cmd.offset;
            segment.size   = cmd.size;
            range(segment, p.payloadSize, "usbload");
            p.totalDownloadSize += cmd.size;
        }
        p.commands.push_back(cmd);
    }
    if(!end || !hasUpdater || !hasUboot || p.commands.empty()) {
        THROW_INVALID_PARAM_EXCEPTION("EGO BIN script header is incomplete");
    }
    range(p.usbUpdater, p.payloadSize, "USB_UPDATER");
    range(p.uboot, p.payloadSize, "U-BOOT");
    p.totalDownloadSize += p.usbUpdater.size + p.uboot.size;
    return p;
}
}  // namespace libobsensor
