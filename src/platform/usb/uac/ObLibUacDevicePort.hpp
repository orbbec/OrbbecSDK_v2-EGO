// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"

#include "ISourcePort.hpp"
#include "stream/StreamProfile.hpp"
#include "usb/enumerator/IUsbEnumerator.hpp"

#include <libusb.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace libobsensor {

// libusb-based USB Audio Class (UAC) capture port for the microphone (Linux / Android / macOS path).
// Reads the isochronous IN endpoint of the AudioStreaming interface directly, bypassing the OS audio
// stack (no ALSA/AAudio). Models the IVideoStreamPort contract so it plugs into AudioSensor unchanged.
//
// NOTE: iso capture (especially on Android usbfs) and detaching snd-usb-audio are high-risk items that
// must be validated by the §8 PoC before relying on this in production.
class ObLibUacDevicePort : public IVideoStreamPort, public IVendorDataPort {
public:
    ObLibUacDevicePort(std::shared_ptr<IUsbDevice> usbDev, std::shared_ptr<const USBSourcePortInfo> portInfo);
    ~ObLibUacDevicePort() noexcept override;

    StreamProfileList getStreamProfileList() override;
    void              startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override;
    void              stopStream(std::shared_ptr<const StreamProfile> profile) override;
    void              stopAllStream() override;

    // IVendorDataPort: not used by UAC, returns 0.
    uint32_t sendAndReceive(const uint8_t *sendData, uint32_t sendLen, uint8_t *recvData, uint32_t exceptedRecvLen, utils::TransferTiming *timing) override;

    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;

private:
    // Parse the configuration descriptor to locate the AudioStreaming interface, its iso IN endpoint,
    // the alt setting carrying audio, the Feature Unit ID and Clock Source ID.
    void parseUacTopology();
    // Tell the device which sampling frequency to stream at (UAC1: endpoint control, UAC2: clock source).
    // Without this the device leaves the iso IN endpoint idle and never delivers audio frames.
    bool setSamplingFrequency(uint32_t sampleRate);

    static void LIBUSB_CALL onIsoTransfer(struct libusb_transfer *transfer);
    void                    eventLoop();
    void                    handleIsoTransfer(struct libusb_transfer *transfer);

private:
    std::shared_ptr<IUsbDevice>              usbDev_;
    std::shared_ptr<const USBSourcePortInfo> portInfo_;
    libusb_device_handle                    *devHandle_ = nullptr;
    libusb_context                          *libusbCtx_ = nullptr;

    // UAC topology (parsed from the config descriptor)
    uint8_t  asInterfaceNumber_ = 0;  // AudioStreaming interface number
    uint8_t  asAltSetting_      = 1;  // alt setting carrying the iso IN endpoint (>=1)
    uint8_t  isoEndpointAddr_   = 0;
    uint16_t isoMaxPacketSize_  = 0;
    uint8_t  featureUnitId_     = 0;
    uint8_t  clockSourceId_     = 0;
    uint8_t  acInterfaceNumber_ = 0;  // AudioControl interface number (for control transfers)
    uint16_t uacBcdVersion_     = 0x0100;
    bool     kernelDetached_    = false;

    // Audio format parsed from the AS Type I FORMAT_TYPE descriptor (fallbacks keep the legacy 16 kHz mono).
    uint32_t sampleRate_    = 16000;
    uint8_t  channelCount_  = 1;
    uint8_t  bitsPerSample_ = 16;

    std::shared_ptr<const AudioStreamProfile> activatedProfile_;
    MutableFrameCallback                      frameCallback_;

    static constexpr int kNumTransfers  = 4;
    static constexpr int kNumIsoPackets = 32;

    std::vector<struct libusb_transfer *> transfers_;
    std::vector<std::vector<uint8_t>>     transferBuffers_;

    std::atomic<bool> streaming_{ false };
    std::thread       eventThread_;
    std::mutex        streamMutex_;

    // Timestamp anchoring: first-sample steady anchor + accumulated sample count (plan §5.4).
    uint64_t              anchorSteadyUs_ = 0;
    std::atomic<uint64_t> totalSamples_{ 0 };
};

}  // namespace libobsensor
