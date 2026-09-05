// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IUsbAudioPlaybackPort.hpp"
#include "usb/enumerator/IUsbEnumerator.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <libusb.h>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace libobsensor {

class ObLibUacPlaybackPort : public IUsbAudioPlaybackPort {
public:
    ObLibUacPlaybackPort(std::shared_ptr<IUsbDevice> usbDev, std::shared_ptr<const USBSourcePortInfo> portInfo);
    ~ObLibUacPlaybackPort() noexcept override;

    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;
    void openSession() override;
    void beginPlayback(uint64_t generation) override;
    size_t writePcm(const uint8_t *data, size_t size, uint64_t generation) override;
    bool drain(uint64_t generation) override;
    void endPlayback(uint64_t generation) noexcept override;
    void closeSession() noexcept override;

private:
    void parseTopology();
    void eventLoop();
    size_t fillTransfer(libusb_transfer *transfer);
    void cancelTransfers();
    bool waitForTransfers() noexcept;
    static void LIBUSB_CALL onTransfer(libusb_transfer *transfer);
    void handleTransfer(libusb_transfer *transfer);

    std::shared_ptr<IUsbDevice> usbDev_;
    std::shared_ptr<const USBSourcePortInfo> portInfo_;
    libusb_device_handle *handle_ = nullptr;
    libusb_context *context_ = nullptr;
    uint8_t interface_ = 0;
    uint8_t altSetting_ = 0;
    uint8_t endpoint_ = 0;
    uint16_t packetBytes_ = 0;
    bool claimed_ = false;
    bool detached_ = false;

    std::mutex mutex_;
    std::condition_variable queueCv_;
    std::condition_variable drainCv_, cbCv_;
    std::deque<uint8_t> queue_;
    static constexpr size_t kMaxQueueBytes = 32000 / 5; // 200 ms at 16 kHz mono S16LE
    std::atomic<bool> sessionOpen_{ false };
    std::atomic<bool> playing_{ false };
    uint64_t generation_ = 0;
    std::atomic<int> inflight_{ 0 };
    std::vector<libusb_transfer *> transfers_;
    std::vector<std::vector<uint8_t>> buffers_;
    std::unordered_map<libusb_transfer *, size_t> submittedAudioBytes_;
    std::mutex cbMutex_;
    std::thread eventThread_;
};

}  // namespace libobsensor
