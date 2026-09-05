// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ObLibUacPlaybackPort.hpp"

#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include "usb/enumerator/UsbEnumeratorLibusb.hpp"

#include <chrono>
#include <string>
#include <utility>

namespace libobsensor {
namespace {
constexpr uint8_t kAudioClass = 1, kAudioStreaming = 2, kCsInterface = 0x24, kFormatType = 0x02;
constexpr int kTransfers = 4, kPackets = 8;
constexpr uint32_t kSampleRate = 16000;
uint32_t readLe24(const uint8_t *p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16); }

const char *transferStatusName(libusb_transfer_status status) {
    switch(status) {
    case LIBUSB_TRANSFER_COMPLETED: return "completed";
    case LIBUSB_TRANSFER_ERROR: return "error";
    case LIBUSB_TRANSFER_TIMED_OUT: return "timed_out";
    case LIBUSB_TRANSFER_CANCELLED: return "cancelled";
    case LIBUSB_TRANSFER_STALL: return "stall";
    case LIBUSB_TRANSFER_NO_DEVICE: return "no_device";
    case LIBUSB_TRANSFER_OVERFLOW: return "overflow";
    default: return "unknown";
    }
}

void logFailedIsoPackets(const libusb_transfer *transfer) {
    for(int i = 0; i < transfer->num_iso_packets; ++i) {
        const auto &packet = transfer->iso_packet_desc[i];
        if(packet.status != LIBUSB_TRANSFER_COMPLETED) {
            LOG_WARN("UAC playback: ISO OUT packet failed: endpoint={}, packet={}, status={} ({}), actual_length={}, expected_length={}",
                     static_cast<unsigned int>(transfer->endpoint), i, static_cast<int>(packet.status), transferStatusName(packet.status),
                     packet.actual_length, packet.length);
        }
    }
}
}  // namespace

ObLibUacPlaybackPort::ObLibUacPlaybackPort(std::shared_ptr<IUsbDevice> dev, std::shared_ptr<const USBSourcePortInfo> info)
    : usbDev_(std::move(dev)), portInfo_(std::move(info)) {
    auto libusbDev = std::dynamic_pointer_cast<UsbDeviceLibusb>(usbDev_);
    if(!libusbDev) THROW_INVALID_PARAM_EXCEPTION("UAC playback requires a libusb-backed USB device");
    handle_ = libusbDev->getLibusbDeviceHandle(); context_ = libusbDev->getLibusbContext();
#if defined(__ANDROID__)
    const auto logLevelResult = libusb_set_option(context_, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_ERROR);
    if(logLevelResult != LIBUSB_SUCCESS) {
        LOG_WARN("UAC playback: unable to enable libusb error logging: result={} ({})", logLevelResult, libusb_error_name(logLevelResult));
    }
#endif
}
ObLibUacPlaybackPort::~ObLibUacPlaybackPort() noexcept { closeSession(); }
std::shared_ptr<const SourcePortInfo> ObLibUacPlaybackPort::getSourcePortInfo() const { return portInfo_; }

void ObLibUacPlaybackPort::parseTopology() {
    libusb_config_descriptor *config = nullptr;
    if(libusb_get_active_config_descriptor(libusb_get_device(handle_), &config) != LIBUSB_SUCCESS || !config)
        THROW_IO_EXCEPTION("UAC playback: cannot read active USB configuration");
    bool found = false;
    for(uint8_t i = 0; i < config->bNumInterfaces && !found; ++i) for(int a = 0; a < config->interface[i].num_altsetting && !found; ++a) {
        const auto &alt = config->interface[i].altsetting[a];
        if(alt.bInterfaceClass != kAudioClass || alt.bInterfaceSubClass != kAudioStreaming || alt.bAlternateSetting == 0) continue;
        bool correctFormat = false;
        for(int off = 0; off + 8 <= alt.extra_length;) {
            const auto len = alt.extra[off];
            if(len < 3 || off + len > alt.extra_length) break;
            if(alt.extra[off + 1] == kCsInterface && alt.extra[off + 2] == kFormatType && len >= 11 && alt.extra[off + 4] == 1
               && alt.extra[off + 5] == 2 && alt.extra[off + 6] == 16) {
                const auto count = alt.extra[off + 7];
                for(uint8_t r = 0; r < count && 8 + r * 3 + 3 <= len; ++r)
                    if(readLe24(alt.extra + off + 8 + r * 3) == kSampleRate) correctFormat = true;
            }
            off += len;
        }
        if(!correctFormat) continue;
        for(uint8_t e = 0; e < alt.bNumEndpoints; ++e) {
            const auto &ep = alt.endpoint[e];
            if((ep.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT
               && (ep.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS) {
                interface_ = alt.bInterfaceNumber; altSetting_ = alt.bAlternateSetting; endpoint_ = ep.bEndpointAddress;
                packetBytes_ = ep.wMaxPacketSize & 0x07ff; found = packetBytes_ >= 32;
                LOG_DEBUG("UAC playback: selected ISO OUT endpoint: interface={}, alt_setting={}, endpoint={}, max_packet_size={}, attributes={}, interval={}, sync_endpoint={}",
                          static_cast<unsigned int>(interface_), static_cast<unsigned int>(altSetting_), static_cast<unsigned int>(endpoint_), packetBytes_,
                          static_cast<unsigned int>(ep.bmAttributes), static_cast<unsigned int>(ep.bInterval), static_cast<unsigned int>(ep.bSynchAddress));
                break;
            }
        }
    }
    libusb_free_config_descriptor(config);
    if(!found) THROW_UNSUPPORTED_OPERATION_EXCEPTION("EGO USB speaker must expose UAC1 16 kHz mono S16LE ISO OUT with 32-byte packets");
}

void ObLibUacPlaybackPort::openSession() {
    if(sessionOpen_) return;
    const auto sessionStart = std::chrono::steady_clock::now();
    parseTopology();
    if(libusb_kernel_driver_active(handle_, interface_) == 1) {
        if(libusb_detach_kernel_driver(handle_, interface_) != LIBUSB_SUCCESS) THROW_IO_EXCEPTION("UAC playback: detach kernel driver failed");
        detached_ = true;
    }
    const auto claimResult = libusb_claim_interface(handle_, interface_);
    if(claimResult != LIBUSB_SUCCESS) {
        if(detached_) {
            libusb_attach_kernel_driver(handle_, interface_);
            detached_ = false;
        }
        THROW_IO_EXCEPTION("UAC playback: claim interface failed: " + std::string(libusb_error_name(claimResult)));
    }
    claimed_ = true;
    const auto altSettingResult = libusb_set_interface_alt_setting(handle_, interface_, altSetting_);
    if(altSettingResult != LIBUSB_SUCCESS) { closeSession(); THROW_IO_EXCEPTION("UAC playback: select streaming alternate setting failed: " + std::string(libusb_error_name(altSettingResult))); }
    uint8_t rate[3] = { 0x80, 0x3e, 0 };
    const auto requestType = static_cast<uint8_t>(static_cast<uint8_t>(LIBUSB_ENDPOINT_OUT) | static_cast<uint8_t>(LIBUSB_REQUEST_TYPE_CLASS)
                                                  | static_cast<uint8_t>(LIBUSB_RECIPIENT_ENDPOINT));
    const auto samplingRateResult = libusb_control_transfer(handle_, requestType, 0x01, 0x0100, endpoint_, rate, sizeof(rate), 1000);
    if(samplingRateResult < 0) { closeSession(); THROW_IO_EXCEPTION("UAC playback: select 16 kHz sampling frequency failed: " + std::string(libusb_error_name(samplingRateResult))); }
    sessionOpen_ = true; eventThread_ = std::thread(&ObLibUacPlaybackPort::eventLoop, this);
    const auto sessionSetupMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - sessionStart).count();
    LOG_DEBUG("UAC playback: session opened in {} ms: interface={}, alt_setting={}, endpoint={}, SET_CUR_result={}", sessionSetupMs,
              static_cast<unsigned int>(interface_), static_cast<unsigned int>(altSetting_), static_cast<unsigned int>(endpoint_), samplingRateResult);
}

void ObLibUacPlaybackPort::beginPlayback(uint64_t generation) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!sessionOpen_) THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("UAC playback session is not open");
    if(playing_) THROW_RESOURCE_BUSY_EXCEPTION("UAC playback is already active");
    if(!transfers_.empty()) THROW_IO_EXCEPTION("UAC playback transport did not finish cancellation; call stopAudioPlayback before retrying");
    generation_ = generation; playing_ = true; queue_.clear();
    buffers_.assign(kTransfers, std::vector<uint8_t>(packetBytes_ * kPackets)); transfers_.assign(kTransfers, nullptr);
    for(int i = 0; i < kTransfers; ++i) {
        auto *transfer = libusb_alloc_transfer(kPackets);
        if(!transfer) { playing_ = false; cancelTransfers(); THROW_MEMORY_EXCEPTION("UAC playback: ISO transfer allocation failed"); }
        libusb_fill_iso_transfer(transfer, handle_, endpoint_, buffers_[i].data(), int(buffers_[i].size()), kPackets, &ObLibUacPlaybackPort::onTransfer, this, 1000);
        libusb_set_iso_packet_lengths(transfer, packetBytes_); transfers_[i] = transfer;
        const auto audioBytes = fillTransfer(transfer);
        const auto submitResult = libusb_submit_transfer(transfer);
        if(submitResult == LIBUSB_SUCCESS) {
            std::lock_guard<std::mutex> cb(cbMutex_); submittedAudioBytes_[transfer] = audioBytes; ++inflight_;
        }
        else {
            LOG_WARN("UAC playback: initial ISO OUT transfer submission failed: endpoint={}, transfer={}, libusb_result={} ({})",
                     static_cast<unsigned int>(endpoint_), i, submitResult, libusb_error_name(submitResult));
            libusb_free_transfer(transfer); transfers_[i] = nullptr;
        }
    }
    if(inflight_ == 0) {
        playing_ = false;
        THROW_IO_EXCEPTION("UAC playback: no ISO OUT transfer could be submitted");
    }
}
size_t ObLibUacPlaybackPort::writePcm(const uint8_t *data, size_t size, uint64_t generation) {
    std::unique_lock<std::mutex> lock(mutex_);
    queueCv_.wait(lock, [&] { return !playing_ || generation_ != generation || queue_.size() + size <= kMaxQueueBytes; });
    if(!playing_ || generation_ != generation) return 0;
    queue_.insert(queue_.end(), data, data + size); return size;
}
bool ObLibUacPlaybackPort::drain(uint64_t generation) {
    std::unique_lock<std::mutex> lock(mutex_);
    drainCv_.wait(lock, [&] {
        if(!playing_ || generation_ != generation || !queue_.empty()) return !playing_ || generation_ != generation;
        std::lock_guard<std::mutex> cb(cbMutex_);
        for(const auto &entry: submittedAudioBytes_) if(entry.second) return false;
        return true;
    });
    return playing_ && generation_ == generation;
}
void ObLibUacPlaybackPort::endPlayback(uint64_t generation) noexcept {
    { std::lock_guard<std::mutex> lock(mutex_); if(generation_ != generation || !playing_) return; playing_ = false; queue_.clear(); queueCv_.notify_all(); drainCv_.notify_all(); }
    cancelTransfers();
    if(waitForTransfers()) { std::lock_guard<std::mutex> cb(cbMutex_); transfers_.clear(); buffers_.clear(); submittedAudioBytes_.clear(); }
}
void ObLibUacPlaybackPort::cancelTransfers() { std::lock_guard<std::mutex> cb(cbMutex_); for(auto *transfer: transfers_) if(transfer) libusb_cancel_transfer(transfer); }
bool ObLibUacPlaybackPort::waitForTransfers() noexcept {
    std::unique_lock<std::mutex> cb(cbMutex_);
    cbCv_.wait(cb, [this] { return inflight_ == 0; });
    return true;
}
void ObLibUacPlaybackPort::closeSession() noexcept {
    endPlayback(generation_); sessionOpen_ = false; cancelTransfers(); const auto reaped = waitForTransfers();
    if(eventThread_.joinable()) eventThread_.join();
    if(reaped) { std::lock_guard<std::mutex> cb(cbMutex_); transfers_.clear(); buffers_.clear(); submittedAudioBytes_.clear(); }
    if(claimed_) { libusb_set_interface_alt_setting(handle_, interface_, 0); libusb_release_interface(handle_, interface_); claimed_ = false; }
    if(detached_) { if(libusb_kernel_driver_active(handle_, interface_) == 0) libusb_attach_kernel_driver(handle_, interface_); detached_ = false; }
}
void ObLibUacPlaybackPort::eventLoop() {
    while(sessionOpen_ || inflight_ > 0) {
        timeval timeout { 0, 100000 };
        libusb_handle_events_timeout_completed(context_, &timeout, nullptr);
    }
}
void LIBUSB_CALL ObLibUacPlaybackPort::onTransfer(libusb_transfer *transfer) { static_cast<ObLibUacPlaybackPort *>(transfer->user_data)->handleTransfer(transfer); }
size_t ObLibUacPlaybackPort::fillTransfer(libusb_transfer *transfer) {
    size_t audioBytes = 0;
    for(int i = 0; i < transfer->length; ++i) { if(queue_.empty()) transfer->buffer[i] = 0; else { transfer->buffer[i] = queue_.front(); queue_.pop_front(); ++audioBytes; } }
    queueCv_.notify_all(); return audioBytes;
}
void ObLibUacPlaybackPort::handleTransfer(libusb_transfer *transfer) {
    { std::lock_guard<std::mutex> cb(cbMutex_); submittedAudioBytes_[transfer] = 0; }
    bool resubmit = false; size_t audioBytes = 0;
    bool wasPlaying = false;
    { std::lock_guard<std::mutex> lock(mutex_); wasPlaying = playing_; if(wasPlaying && (transfer->status == LIBUSB_TRANSFER_COMPLETED || transfer->status == LIBUSB_TRANSFER_TIMED_OUT)) { audioBytes = fillTransfer(transfer); resubmit = true; } drainCv_.notify_all(); }
    if(resubmit) {
        logFailedIsoPackets(transfer);
        // Publish the next payload before submitting it.  drain() must not observe an
        // empty queue during this small window and cancel the last real PCM packet.
        { std::lock_guard<std::mutex> cb(cbMutex_); submittedAudioBytes_[transfer] = audioBytes; }
        const auto submitResult = libusb_submit_transfer(transfer);
        if(submitResult == LIBUSB_SUCCESS) return;
        LOG_WARN("UAC playback: ISO OUT transfer resubmission failed: endpoint={}, status={} ({}), libusb_result={} ({})",
                 static_cast<unsigned int>(transfer->endpoint), static_cast<int>(transfer->status), transferStatusName(transfer->status), submitResult,
                 libusb_error_name(submitResult));
    }
    else if(wasPlaying) {
        LOG_WARN("UAC playback: ISO OUT transfer stopped: endpoint={}, status={} ({})", static_cast<unsigned int>(transfer->endpoint),
                 static_cast<int>(transfer->status), transferStatusName(transfer->status));
        logFailedIsoPackets(transfer);
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // A terminal USB error must unblock a producer waiting on queue capacity;
        // otherwise playFile() could wait forever after a device disconnect.
        playing_ = false;
        queue_.clear();
        queueCv_.notify_all();
        drainCv_.notify_all();
    }
    { std::lock_guard<std::mutex> cb(cbMutex_); for(auto *&item: transfers_) if(item == transfer) { item = nullptr; break; } submittedAudioBytes_.erase(transfer); libusb_free_transfer(transfer); --inflight_; if(inflight_ == 0) cbCv_.notify_all(); }
}
}  // namespace libobsensor
