// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// EgoPro automated self-test: exercises the main device functions without any GUI and reports a
// PASS/FAIL/SKIP result for each, then prints a summary and returns a non-zero exit code on any
// failure so it can be dropped into CI.
//
// Tested functions:
//   - Device info      : version, serial number, temperature, TF-card capacity
//   - Control commands : read + write/verify/restore sweep over every supported property (int/float/
//                        bool write-verified; struct/raw-data commands read-only probed)
//   - Video frames     : every available color stream (left/right/side) delivers frames
//   - IMU              : accel + gyro frames are delivered
//   - Audio            : microphone frames are delivered
//   - Ego state        : the HID work-state callback fires
//   - Battery          : the HID battery-info callback fires
//
// Usage: ob_ego_auto_test [ip] [port] [durationSec] [reportFile]
//   ip          - device IP address; when omitted the first enumerated device is used
//   port        - vendor control port (default: 8090)
//   durationSec - per-stream capture window in seconds (default: 5)
//   reportFile  - path of the .txt report file (default: <device-name>_auto_test_report.txt)

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

// A stream buffer that forwards everything written to it to two underlying buffers at once, so the
// test output can go to both the console and a report file without touching any of the print calls.
class TeeBuf : public std::streambuf {
public:
    TeeBuf(std::streambuf *first, std::streambuf *second) : first_(first), second_(second) {}

protected:
    int overflow(int ch) override {
        if(ch == EOF) {
            return !EOF;
        }
        const int r1 = first_->sputc(static_cast<char>(ch));
        const int r2 = second_->sputc(static_cast<char>(ch));
        return (r1 == EOF || r2 == EOF) ? EOF : ch;
    }
    int sync() override {
        const int r1 = first_->pubsync();
        const int r2 = second_->pubsync();
        return (r1 == 0 && r2 == 0) ? 0 : -1;
    }

private:
    std::streambuf *first_;
    std::streambuf *second_;
};

// ============================ Test result bookkeeping ============================
enum class Status { PASS, FAIL, SKIP };

struct TestResult {
    std::string name;
    Status      status;
    std::string detail;
};

std::vector<TestResult> g_results;

void record(const std::string &name, Status status, const std::string &detail) {
    const char *tag = status == Status::PASS ? "PASS" : (status == Status::FAIL ? "FAIL" : "SKIP");
    std::cout << "[" << tag << "] " << name;
    if(!detail.empty()) {
        std::cout << " - " << detail;
    }
    std::cout << std::endl;
    g_results.push_back({ name, status, detail });
}

// ============================ Formatting helpers ============================
const char *egoWorkStateToString(OBEgoWorkState state) {
    switch(state) {
    case OB_EGO_WORK_STATE_BOOTING:
        return "BOOTING";
    case OB_EGO_WORK_STATE_SELF_TESTING:
        return "SELF_TESTING";
    case OB_EGO_WORK_STATE_READY:
        return "READY";
    case OB_EGO_WORK_STATE_RECORD_COUNTDOWN:
        return "RECORD_COUNTDOWN";
    case OB_EGO_WORK_STATE_RECORDING:
        return "RECORDING";
    case OB_EGO_WORK_STATE_STOPPING_RECORD:
        return "STOPPING_RECORD";
    case OB_EGO_WORK_STATE_SAVING:
        return "SAVING";
    case OB_EGO_WORK_STATE_FW_UPGRADING:
        return "FW_UPGRADING";
    case OB_EGO_WORK_STATE_SHUTTING_DOWN:
        return "SHUTTING_DOWN";
    case OB_EGO_WORK_STATE_FAULT:
        return "FAULT";
    case OB_EGO_WORK_STATE_RECORD_SAVED:
        return "RECORD_SAVED";
    default:
        return "UNKNOWN";
    }
}

bool hasSensorType(const std::shared_ptr<ob::Device> &device, OBSensorType sensorType) {
    auto sensorList = device->getSensorList();
    for(uint32_t index = 0; index < sensorList->getCount(); index++) {
        if(sensorList->getSensorType(index) == sensorType) {
            return true;
        }
    }
    return false;
}

// Build a filesystem-safe report file name from the device name so that Ego and EgoPro runs do not
// share one file, e.g. "Orbbec EgoPro" -> "OrbbecEgoPro_auto_test_report.txt".
std::string makeReportFileName(const std::shared_ptr<ob::Device> &device) {
    std::string base;
    try {
        base = device->getDeviceInfo()->getName();
    }
    catch(const ob::Error &) {
    }
    std::string sanitized;
    for(char c: base) {
        if(std::isalnum(static_cast<unsigned char>(c))) {
            sanitized.push_back(c);
        }
        else if(c == ' ' || c == '-' || c == '_') {
            sanitized.push_back('_');
        }
        // other punctuation is dropped
    }
    if(sanitized.empty()) {
        sanitized = "ego";
    }
    return sanitized + "_auto_test_report.txt";
}

// ============================ Individual tests ============================
void testDeviceInfo(const std::shared_ptr<ob::Device> &device) {
    const std::string name = "Device info";
    try {
        auto info = device->getDeviceInfo();
        std::cout << "  name=" << info->getName() << " sn=" << info->getSerialNumber() << " pid=0x" << std::hex << info->getPid() << std::dec
                  << " fw=" << info->getFirmwareVersion() << std::endl;

        // Temperature (read-only structured data).
        if(device->isPropertySupported(OB_STRUCT_DEVICE_TEMPERATURE, OB_PERMISSION_READ)) {
            OBDeviceTemperature temp     = {};
            uint32_t            dataSize = sizeof(temp);
            device->getStructuredData(OB_STRUCT_DEVICE_TEMPERATURE, reinterpret_cast<uint8_t *>(&temp), &dataSize);
            std::cout << "  temperature: cpu=" << temp.cpuTemp << "C imu=" << temp.imuTemp << "C rgb=" << temp.rgbTemp << "C" << std::endl;
        }

        // TF card capacity (read-only structured data).
        if(device->isPropertySupported(OB_STRUCT_GET_TF_CARD_CAPACITY, OB_PERMISSION_READ)) {
            OBTFCardCapacity tf       = {};
            uint32_t         dataSize = sizeof(tf);
            device->getStructuredData(OB_STRUCT_GET_TF_CARD_CAPACITY, reinterpret_cast<uint8_t *>(&tf), &dataSize);
            std::cout << "  TF card: status=" << tf.tfCardStatus << " total=" << tf.totalCapacity << "GB remain=" << tf.remainCapacity << "GB" << std::endl;
        }

        const std::string sn = device->getDeviceInfo()->getSerialNumber();
        record(name, sn.empty() ? Status::FAIL : Status::PASS, sn.empty() ? "serial number is empty" : "");
    }
    catch(const ob::Error &e) {
        record(name, Status::FAIL, e.what());
    }
}

// Property IDs whose *write* triggers a device-level side effect (factory reset, reboot delay,
// start/stop recording). We still read them, but never write them during the automated sweep.
bool isWriteUnsafe(OBPropertyID id) {
    switch(id) {
    case OB_PROP_RESTORE_FACTORY_SETTINGS_BOOL:
    case OB_PROP_DEVICE_REBOOT_DELAY_INT:
    case OB_PROP_RECORDING_ACTION_INT:  // alias: OB_PROP_RECORDING_PROMPT_INT
        return true;
    default:
        return false;
    }
}

// Some numeric properties only take effect once an "auto"/"dynamic" switch is turned off first
// (e.g. manual exposure needs auto-exposure off). This table maps such a property to the bool gate
// that must hold a given value before the write is attempted.
struct WriteGate {
    OBPropertyID target;
    OBPropertyID gate;
    bool         gateValue;
};

const WriteGate kWriteGates[] = {
    { OB_PROP_COLOR_EXPOSURE_INT, OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, false },
    { OB_PROP_COLOR_GAIN_INT, OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, false },
    { OB_PROP_COLOR_WHITE_BALANCE_INT, OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, false },
    { OB_PROP_COLOR_BITRATE_INT, OB_PROP_COLOR_DYNAMIC_BITRATE_ENABLE_BOOL, false },
};

bool findWriteGate(OBPropertyID id, OBPropertyID *gateId, bool *gateValue) {
    for(const auto &g: kWriteGates) {
        if(g.target == id) {
            *gateId    = g.gate;
            *gateValue = g.gateValue;
            return true;
        }
    }
    return false;
}

// RAII helper: temporarily forces a bool gate property to the value a dependent write needs, and
// restores the gate's original value when it goes out of scope (even if the write throws).
struct GateGuard {
    std::shared_ptr<ob::Device> device;
    OBPropertyID                gateId   = OBPropertyID(0);
    bool                        original = false;
    bool                        applied  = false;

    explicit GateGuard(const std::shared_ptr<ob::Device> &dev) : device(dev) {}

    void apply(OBPropertyID id, bool value) {
        try {
            if(!device->isPropertySupported(id, OB_PERMISSION_READ) || !device->isPropertySupported(id, OB_PERMISSION_WRITE)) {
                return;
            }
            original = device->getBoolProperty(id);
            if(original != value) {
                device->setBoolProperty(id, value);
                gateId  = id;
                applied = true;
            }
        }
        catch(const ob::Error &) {
            // If the gate itself cannot be toggled, fall through; the write test will report the result.
        }
    }

    ~GateGuard() {
        if(applied) {
            try {
                device->setBoolProperty(gateId, original);
            }
            catch(const ob::Error &) {
            }
        }
    }
};

const char *permissionToString(OBPermissionType permission) {
    switch(permission) {
    case OB_PERMISSION_READ:
        return "R/_";
    case OB_PERMISSION_WRITE:
        return "_/W";
    case OB_PERMISSION_READ_WRITE:
        return "R/W";
    default:
        return "_/_";
    }
}

const char *propertyTypeToString(OBPropertyType type) {
    switch(type) {
    case OB_BOOL_PROPERTY:
        return "bool";
    case OB_INT_PROPERTY:
        return "int";
    case OB_FLOAT_PROPERTY:
        return "float";
    default:
        return "struct";
    }
}

// Aggregate counters for the property sweep.
struct ControlStats {
    int total         = 0;  // primary-type, non-DENY properties examined
    int readOk        = 0;
    int readFail      = 0;  // a supported read raised -> genuine failure
    int writeOk       = 0;
    int writeMismatch = 0;  // write accepted but readback differed -> genuine failure
    int writeWarn     = 0;  // write/restore raised -> likely state-dependent, reported as warning only
    int writeSkip     = 0;  // not readable / single-value range / side-effecting
};

// Raw-data property IDs (read via the chunked getRawData protocol rather than getStructuredData).
// Internal-only raw IDs that are not in the public header still work through the struct->raw fallback
// below; this list just lets us pick the correct path directly for the known public ones.
bool isKnownRawDataProperty(OBPropertyID id) {
    switch(id) {
    case OB_RAW_DATA_ALIGN_CALIB_YAML:
    case OB_RAW_DATA_IMU_CALIB_YAML:
    case OB_RAW_DATA_EXPORT_LOG:
    case OB_RAW_DATA_CAMERA_CALIB_JSON_FILE:
    case OB_RAW_DATA_FW_RECORDING_MCAP_TOPIC_LIST:
    case OB_RAW_DATA_FW_RECORDING_PART_LIST:
        return true;
    default:
        return false;
    }
}

std::string bytesToString(const uint8_t *data, size_t size) {
    size_t length = 0;
    while(length < size && data[length] != 0) {
        ++length;
    }
    for(size_t i = 0; i < length; ++i) {
        if(!std::isprint(static_cast<unsigned char>(data[i]))) {
            return std::string();
        }
    }
    return std::string(reinterpret_cast<const char *>(data), length);
}

// Read a struct property into a buffer; returns true and the byte count on success.
bool readStructData(const std::shared_ptr<ob::Device> &device, OBPropertyID id, uint32_t *outSize, std::string *value, std::string *err) {
    try {
        std::vector<uint8_t> buffer(4096);
        uint32_t             size = static_cast<uint32_t>(buffer.size());
        device->getStructuredData(id, buffer.data(), &size);
        *outSize = size;
        *value   = size <= 32 ? bytesToString(buffer.data(), size) : std::string();
        return true;
    }
    catch(const ob::Error &e) {
        *err = e.what();
        return false;
    }
}

// Read a raw-data property via the chunked transfer protocol; returns true and the full byte count on
// success. A negative transfer state or an exception is reported as a failure.
bool readRawData(const std::shared_ptr<ob::Device> &device, OBPropertyID id, uint32_t *outSize, std::string *value, std::string *err) {
    uint32_t    total  = 0;
    bool        failed = false;
    std::string localErr;
    std::vector<uint8_t> data;
    try {
        device->getRawData(id, [&](OBDataTranState state, OBDataChunk *chunk) {
            if(chunk != nullptr && chunk->fullDataSize > total) {
                total = chunk->fullDataSize;
                data.resize(total);
            }
            if(chunk != nullptr && chunk->data != nullptr && chunk->offset < data.size()) {
                const auto copySize = std::min<size_t>(chunk->size, data.size() - chunk->offset);
                std::memcpy(data.data() + chunk->offset, chunk->data, copySize);
            }
            if(static_cast<int>(state) < 0) {  // negative states are error codes
                failed   = true;
                localErr = "transfer state " + std::to_string(static_cast<int>(state));
            }
        });
    }
    catch(const ob::Error &e) {
        *err = e.what();
        return false;
    }
    if(failed) {
        *err = localErr;
        return false;
    }
    *outSize = total;
    *value   = total <= 32 ? bytesToString(data.data(), data.size()) : std::string();
    return true;
}

// Read-only probe for a struct / raw-data property. Writes are skipped: they need a valid payload and
// could corrupt calibration / recording / IP configuration.
void testStructOrRawProperty(const std::shared_ptr<ob::Device> &device, const OBPropertyItem &item, ControlStats &st) {
    ++st.total;
    const bool readable = (item.permission & OB_PERMISSION_READ) != 0;
    const bool writable = (item.permission & OB_PERMISSION_WRITE) != 0;

    std::cout << "  [" << permissionToString(item.permission) << "] " << item.name << " (struct/raw, id=" << item.id << ")";

    if(readable) {
        uint32_t    size = 0;
        std::string err;
        std::string value;
        bool        ok = false;
        if(isKnownRawDataProperty(item.id)) {
            ok = readRawData(device, item.id, &size, &value, &err);
        }
        else {
            ok = readStructData(device, item.id, &size, &value, &err);
            if(!ok) {
                // Some binary properties are actually raw-data; retry through the chunked path.
                std::string rawErr;
                ok = readRawData(device, item.id, &size, &value, &rawErr);
                if(!ok) {
                    err += "; raw: " + rawErr;
                }
            }
        }
        if(ok) {
            ++st.readOk;
            std::cout << " read-ok (" << size << " bytes)";
            if(size <= 32 && !value.empty()) {
                std::cout << " value=\"" << value << "\"";
            }
        }
        else {
            ++st.readFail;
            std::cout << " read-ERROR: " << err;
        }
    }
    if(writable) {
        ++st.writeSkip;
        std::cout << " write-skip(struct/raw payload)";
    }
    std::cout << std::endl;
}

// Enumerate every supported property (like the 2.device.control sample) and, for each primary-type
// (bool/int/float) property, read it and - when it is both readable and writable and safe - perform a
// write / verify / restore round-trip. Read failures and readback mismatches fail the test; writes the
// device rejects outright are treated as (state-dependent) warnings.
void testControlCommands(const std::shared_ptr<ob::Device> &device) {
    const std::string name = "Control commands";
    ControlStats      st;
    try {
        uint32_t count = device->getSupportedPropertyCount();
        for(uint32_t i = 0; i < count; ++i) {
            OBPropertyItem item = device->getSupportedProperty(i);
            if(item.permission == OB_PERMISSION_DENY) {
                continue;
            }
            if(item.type == OB_STRUCT_PROPERTY) {
                testStructOrRawProperty(device, item, st);  // struct / raw-data: read-only probe
                continue;
            }
            if(item.type != OB_BOOL_PROPERTY && item.type != OB_INT_PROPERTY && item.type != OB_FLOAT_PROPERTY) {
                continue;
            }
            ++st.total;

            const bool readable = (item.permission & OB_PERMISSION_READ) != 0;
            const bool writable = (item.permission & OB_PERMISSION_WRITE) != 0;

            std::cout << "  [" << permissionToString(item.permission) << "] " << item.name << " (" << propertyTypeToString(item.type) << ", id=" << item.id
                      << ")";

            // ---- read ----
            if(readable) {
                try {
                    switch(item.type) {
                    case OB_BOOL_PROPERTY:
                        std::cout << " = " << (device->getBoolProperty(item.id) ? "true" : "false");
                        break;
                    case OB_INT_PROPERTY:
                        std::cout << " = " << device->getIntProperty(item.id);
                        break;
                    case OB_FLOAT_PROPERTY:
                        std::cout << " = " << device->getFloatProperty(item.id);
                        break;
                    default:
                        break;
                    }
                    ++st.readOk;
                }
                catch(const ob::Error &e) {
                    ++st.readFail;
                    std::cout << " read-ERROR: " << e.what();
                }
            }

            // ---- write / verify / restore ----
            if(writable && isWriteUnsafe(item.id)) {
                ++st.writeSkip;
                std::cout << " write-skip(side-effecting)";
            }
            else if(writable && !readable) {
                ++st.writeSkip;
                std::cout << " write-skip(not readable)";
            }
            else if(writable && readable) {
                // Apply any prerequisite gate (e.g. auto-exposure off) for the duration of this write.
                GateGuard    gate(device);
                OBPropertyID gateId;
                bool         gateValue;
                if(findWriteGate(item.id, &gateId, &gateValue)) {
                    gate.apply(gateId, gateValue);
                }
                try {
                    switch(item.type) {
                    case OB_BOOL_PROPERTY: {
                        bool original = device->getBoolProperty(item.id);
                        // Exercise both values explicitly: set 0 then 1, verifying the readback each time.
                        device->setBoolProperty(item.id, false);
                        bool readback0 = device->getBoolProperty(item.id);
                        device->setBoolProperty(item.id, true);
                        bool readback1 = device->getBoolProperty(item.id);
                        device->setBoolProperty(item.id, original);  // restore
                        if(readback0 != false || readback1 != true) {
                            ++st.writeMismatch;
                            std::cout << " write-MISMATCH(set 0 read " << (readback0 ? 1 : 0) << ", set 1 read " << (readback1 ? 1 : 0) << ")";
                        }
                        else {
                            ++st.writeOk;
                            std::cout << " write-ok(0/1 verified)";
                        }
                        break;
                    }
                    case OB_INT_PROPERTY: {
                        auto range = device->getIntPropertyRange(item.id);
                        if(range.min >= range.max) {
                            ++st.writeSkip;
                            std::cout << " write-skip(single-value)";
                            break;
                        }
                        int32_t original = device->getIntProperty(item.id);
                        int32_t target   = (original == range.min) ? range.max : range.min;
                        device->setIntProperty(item.id, target);
                        int32_t readback = device->getIntProperty(item.id);
                        device->setIntProperty(item.id, original);  // restore
                        if(readback != target) {
                            ++st.writeMismatch;
                            std::cout << " write-MISMATCH(wrote " << target << " read " << readback << ")";
                        }
                        else {
                            ++st.writeOk;
                            std::cout << " write-ok";
                        }
                        break;
                    }
                    case OB_FLOAT_PROPERTY: {
                        auto range = device->getFloatPropertyRange(item.id);
                        if(range.min >= range.max) {
                            ++st.writeSkip;
                            std::cout << " write-skip(single-value)";
                            break;
                        }
                        float original = device->getFloatProperty(item.id);
                        float target   = (original == range.min) ? range.max : range.min;
                        device->setFloatProperty(item.id, target);
                        float readback = device->getFloatProperty(item.id);
                        device->setFloatProperty(item.id, original);  // restore
                        if(std::fabs(readback - target) > 1e-3f) {
                            ++st.writeMismatch;
                            std::cout << " write-MISMATCH(wrote " << target << " read " << readback << ")";
                        }
                        else {
                            ++st.writeOk;
                            std::cout << " write-ok";
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }
                catch(const ob::Error &e) {
                    ++st.writeWarn;
                    std::cout << " write-WARN: " << e.what();
                }
            }

            std::cout << std::endl;
        }

        std::cout << "  --- " << st.total << " properties: read ok=" << st.readOk << " fail=" << st.readFail << ", write ok=" << st.writeOk
                  << " mismatch=" << st.writeMismatch << " warn=" << st.writeWarn << " skip=" << st.writeSkip << std::endl;

        bool        ok = (st.readFail == 0 && st.writeMismatch == 0);
        std::string detail;
        if(!ok) {
            detail = std::to_string(st.readFail) + " read failure(s), " + std::to_string(st.writeMismatch) + " write mismatch(es)";
        }
        else if(st.writeWarn > 0) {
            detail = std::to_string(st.writeWarn) + " write(s) rejected by device (likely state-dependent)";
        }
        record(name, ok ? Status::PASS : Status::FAIL, detail);
    }
    catch(const ob::Error &e) {
        record(name, Status::FAIL, e.what());
    }
}

void testVideoStreams(const std::shared_ptr<ob::Device> &device, int durationSec) {
    const std::string name = "Video frames";

    struct StreamEntry {
        OBSensorType sensorType;
        OBStreamType streamType;
        OBFrameType  frameType;
        const char  *label;
    };
    const StreamEntry candidates[] = {
        { OB_SENSOR_COLOR_LEFT, OB_STREAM_COLOR_LEFT, OB_FRAME_COLOR_LEFT, "COLOR_LEFT" },
        { OB_SENSOR_COLOR_RIGHT, OB_STREAM_COLOR_RIGHT, OB_FRAME_COLOR_RIGHT, "COLOR_RIGHT" },
        { OB_SENSOR_COLOR_SIDE_LEFT, OB_STREAM_COLOR_SIDE_LEFT, OB_FRAME_COLOR_SIDE_LEFT, "COLOR_SIDE_LEFT" },
        { OB_SENSOR_COLOR_SIDE_RIGHT, OB_STREAM_COLOR_SIDE_RIGHT, OB_FRAME_COLOR_SIDE_RIGHT, "COLOR_SIDE_RIGHT" },
        { OB_SENSOR_COLOR, OB_STREAM_COLOR, OB_FRAME_COLOR, "COLOR" },
    };

    std::vector<StreamEntry> enabled;
    auto                     config = std::make_shared<ob::Config>();
    for(const auto &c: candidates) {
        if(hasSensorType(device, c.sensorType)) {
            // H264 is the safe encoded default advertised by EgoPro; resolution/fps left to the device.
            config->enableVideoStream(c.streamType, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_H264);
            enabled.push_back(c);
        }
    }

    if(enabled.empty()) {
        record(name, Status::SKIP, "no color sensor present");
        return;
    }

    try {
        ob::Pipeline pipe(device);
        pipe.start(config);

        std::vector<uint64_t> counts(enabled.size(), 0);
        auto                  start = std::chrono::steady_clock::now();
        while(std::chrono::steady_clock::now() - start < std::chrono::seconds(durationSec)) {
            auto frameSet = pipe.waitForFrameset(200);
            if(frameSet == nullptr) {
                continue;
            }
            for(size_t i = 0; i < enabled.size(); ++i) {
                if(frameSet->getFrame(enabled[i].frameType) != nullptr) {
                    ++counts[i];
                }
            }
        }
        pipe.stop();

        bool ok = true;
        for(size_t i = 0; i < enabled.size(); ++i) {
            double fps = counts[i] / static_cast<double>(durationSec);
            std::cout << "  " << enabled[i].label << ": " << counts[i] << " frames (" << std::fixed << std::setprecision(1) << fps << " fps)" << std::endl;
            if(counts[i] == 0) {
                ok = false;
            }
        }
        record(name, ok ? Status::PASS : Status::FAIL, ok ? "" : "one or more enabled streams delivered no frames");
    }
    catch(const ob::Error &e) {
        record(name, Status::FAIL, e.what());
    }
}

void testImu(const std::shared_ptr<ob::Device> &device, int durationSec) {
    const std::string name = "IMU frames";
    if(!hasSensorType(device, OB_SENSOR_ACCEL) || !hasSensorType(device, OB_SENSOR_GYRO)) {
        record(name, Status::SKIP, "accel or gyro sensor not present");
        return;
    }

    try {
        std::atomic<uint64_t> accelCount(0);
        std::atomic<uint64_t> gyroCount(0);

        ob::Pipeline imuPipe(device);
        auto         imuConfig = std::make_shared<ob::Config>();
        imuConfig->enableAccelStream();
        imuConfig->enableGyroStream();
        imuPipe.start(imuConfig, [&accelCount, &gyroCount](std::shared_ptr<ob::FrameSet> frameSet) {
            if(!frameSet) {
                return;
            }
            if(frameSet->getFrame(OB_FRAME_ACCEL) != nullptr) {
                ++accelCount;
            }
            if(frameSet->getFrame(OB_FRAME_GYRO) != nullptr) {
                ++gyroCount;
            }
        });

        std::this_thread::sleep_for(std::chrono::seconds(durationSec));
        imuPipe.stop();

        std::cout << "  accel: " << accelCount.load() << " frames, gyro: " << gyroCount.load() << " frames" << std::endl;
        bool ok = accelCount.load() > 0 && gyroCount.load() > 0;
        record(name, ok ? Status::PASS : Status::FAIL, ok ? "" : "accel or gyro delivered no frames");
    }
    catch(const ob::Error &e) {
        record(name, Status::FAIL, e.what());
    }
}

void testAudio(const std::shared_ptr<ob::Device> &device, int durationSec) {
    const std::string name = "Audio frames";
    if(!hasSensorType(device, OB_SENSOR_AUDIO)) {
        record(name, Status::SKIP, "audio sensor not present");
        return;
    }

    try {
        auto audioSensor = device->getSensor(OB_SENSOR_AUDIO);
        auto profiles    = audioSensor->getStreamProfileList();
        if(!profiles || profiles->getCount() == 0) {
            record(name, Status::FAIL, "audio sensor exposes no stream profile");
            return;
        }

        auto profile = profiles->getProfile(0);
        if(profile->is<ob::AudioStreamProfile>()) {
            auto audioProfile = profile->as<ob::AudioStreamProfile>();
            std::cout << "  audio profile: " << audioProfile->getSampleRate() << " Hz, " << audioProfile->getChannelCount() << " ch, "
                      << audioProfile->getBitsPerSample() << " bit" << std::endl;
        }

        std::atomic<uint64_t> audioCount(0);
        audioSensor->start(profile, [&audioCount](std::shared_ptr<ob::Frame> frame) {
            if(frame && frame->getDataSize() > 0) {
                ++audioCount;
            }
        });

        std::this_thread::sleep_for(std::chrono::seconds(durationSec));
        audioSensor->stop();

        std::cout << "  audio: " << audioCount.load() << " frames" << std::endl;
        record(name, audioCount.load() > 0 ? Status::PASS : Status::FAIL, audioCount.load() > 0 ? "" : "no audio frames delivered");
    }
    catch(const ob::Error &e) {
        record(name, Status::FAIL, e.what());
    }
}

void testEgoState(const std::shared_ptr<ob::DataAcquisitionDevice> &device, int durationSec) {
    const std::string name = "Ego state callback";
    try {
        std::atomic<uint64_t> stateCount(0);
        device->setEgoStateCallback([&stateCount](const OBEgoStateReport &state) {
            if(stateCount.fetch_add(1) == 0) {
                std::cout << "  first state: workState=" << egoWorkStateToString(state.work_state) << " seq=" << state.sequence << " stateFlags=0x" << std::hex
                          << state.state_flags << " errorFlags=0x" << state.error_flags << std::dec << " storageFreeBytes=" << state.storage_free_bytes
                          << std::endl;
            }
        });

        std::this_thread::sleep_for(std::chrono::seconds(durationSec));
        device->clearEgoStateCallback();

        std::cout << "  received " << stateCount.load() << " state reports" << std::endl;
        record(name, stateCount.load() > 0 ? Status::PASS : Status::FAIL, stateCount.load() > 0 ? "" : "no state reports received");
    }
    catch(const ob::Error &e) {
        // Firmware/transport may not expose the HID state stream; treat as skipped rather than failed.
        record(name, Status::SKIP, e.what());
    }
}

void testBattery(const std::shared_ptr<ob::DataAcquisitionDevice> &device, int durationSec) {
    const std::string name = "Battery callback";
    try {
        std::atomic<uint64_t> batteryCount(0);
        device->setBatteryInfoCallback([&batteryCount](const OBBatteryInfo &info) {
            if(batteryCount.fetch_add(1) == 0) {
                std::cout << "  first battery: capacity=" << info.capacity << " voltage=" << info.voltage
                          << " chargingState=" << static_cast<int>(info.chargingState) << " hasBattery=" << static_cast<int>(info.hasBattery)
                          << " temperature=" << static_cast<int>(info.temperature) << std::endl;
            }
        });

        std::this_thread::sleep_for(std::chrono::seconds(durationSec));
        device->clearBatteryInfoCallback();

        std::cout << "  received " << batteryCount.load() << " battery reports" << std::endl;
        // Battery reports are low-rate; absence is not necessarily a defect, so report SKIP rather than FAIL.
        record(name, batteryCount.load() > 0 ? Status::PASS : Status::SKIP, batteryCount.load() > 0 ? "" : "no battery reports received");
    }
    catch(const ob::Error &e) {
        record(name, Status::SKIP, e.what());
    }
}

int printSummary() {
    int passed  = 0;
    int failed  = 0;
    int skipped = 0;
    std::cout << "\n==================== EgoPro auto-test summary ====================" << std::endl;
    for(const auto &r: g_results) {
        const char *tag = r.status == Status::PASS ? "PASS" : (r.status == Status::FAIL ? "FAIL" : "SKIP");
        std::cout << "  " << std::left << std::setw(6) << tag << r.name;
        if(!r.detail.empty()) {
            std::cout << " (" << r.detail << ")";
        }
        std::cout << std::endl;
        if(r.status == Status::PASS) {
            ++passed;
        }
        else if(r.status == Status::FAIL) {
            ++failed;
        }
        else {
            ++skipped;
        }
    }
    std::cout << "------------------------------------------------------------------" << std::endl;
    std::cout << "  total=" << g_results.size() << " passed=" << passed << " failed=" << failed << " skipped=" << skipped << std::endl;
    std::cout << "==================================================================" << std::endl;
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

}  // namespace

int main(int argc, char *argv[]) try {
    const char *ip          = argc > 1 ? argv[1] : nullptr;
    uint16_t    port        = argc > 2 ? static_cast<uint16_t>(std::strtoul(argv[2], nullptr, 10)) : 8090;
    int         durationSec = argc > 3 ? std::atoi(argv[3]) : 5;
    if(durationSec <= 0) {
        durationSec = 5;
    }
    const char *explicitReport = argc > 4 ? argv[4] : nullptr;

    ob::Context ctx;

    // On Linux, EGO USB streaming uses the libuvc backend; harmless for network devices.
#if defined(__linux__) || defined(__ANDROID__)
    ctx.setUvcBackendType(OB_UVC_BACKEND_TYPE_LIBUVC);
#endif

    std::shared_ptr<ob::DataAcquisitionDevice> device;
    if(ip != nullptr) {
        std::cout << "Connecting to network device at " << ip << ":" << port << " ..." << std::endl;
        device = ob::DataAcquisitionDevice::fromDevice(ctx.createNetDevice(ip, port));
    }
    else {
        auto list = ctx.queryDeviceList();
        if(list->getCount() == 0) {
            std::cerr << "No device found. Pass an IP address to connect to a network device: ob_ego_auto_test <ip> [port] [durationSec]" << std::endl;
            return EXIT_FAILURE;
        }
        device = ob::DataAcquisitionDevice::fromDevice(list->getDevice(0));
    }

    // Report file name defaults to one derived from the device name (Ego vs EgoPro) unless overridden.
    const std::string reportPath = explicitReport ? explicitReport : makeReportFileName(device);

    // Mirror all console output into the report file via a tee buffer; restored on exit by the guard.
    std::ofstream           reportFile(reportPath);
    std::streambuf         *originalCout = std::cout.rdbuf();
    std::unique_ptr<TeeBuf> tee;
    if(reportFile.is_open()) {
        tee.reset(new TeeBuf(originalCout, reportFile.rdbuf()));
        std::cout.rdbuf(tee.get());
    }
    else {
        std::cerr << "Warning: cannot open report file '" << reportPath << "', console output only." << std::endl;
    }
    // Restores std::cout before reportFile/tee are destroyed, regardless of how main returns.
    struct CoutGuard {
        std::streambuf *buf;
        ~CoutGuard() {
            std::cout.rdbuf(buf);
        }
    } coutGuard{ originalCout };

    std::cout << "Report file: " << reportPath << std::endl;
    std::cout << "Running EgoPro automated test (per-stream window: " << durationSec << "s)\n" << std::endl;

    testDeviceInfo(device);
    testControlCommands(device);
    testVideoStreams(device, durationSec);
    testImu(device, durationSec);
    testAudio(device, durationSec);
    testEgoState(device, durationSec);
    testBattery(device, durationSec);

    return printSummary();
}
catch(const ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
              << "\ntype:" << e.getExceptionType() << std::endl;
    return EXIT_FAILURE;
}
catch(const std::exception &e) {
    std::cerr << "ego auto-test failed: " << e.what() << std::endl;
    return EXIT_FAILURE;
}
