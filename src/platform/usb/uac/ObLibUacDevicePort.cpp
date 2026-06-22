// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ObLibUacDevicePort.hpp"

#include "frame/FrameFactory.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include "usb/enumerator/UsbEnumeratorLibusb.hpp"

namespace libobsensor {

// UAC class-specific request codes
#define UAC_SET_CUR 0x01
// USB Audio interface subclasses
#define UAC_SUBCLASS_AUDIOCONTROL 0x01
#define UAC_SUBCLASS_AUDIOSTREAMING 0x02
// Class-specific descriptor type
#define UAC_CS_INTERFACE 0x24
// AudioControl interface descriptor subtypes
#define UAC_AC_HEADER 0x01
#define UAC_AC_FEATURE_UNIT 0x06
#define UAC_AC_CLOCK_SOURCE 0x0A  // UAC2
// AudioStreaming interface descriptor subtype + Sampling Frequency control selector
#define UAC_AS_FORMAT_TYPE 0x02
#define UAC_SAMPLING_FREQ_CONTROL 0x01

ObLibUacDevicePort::ObLibUacDevicePort(std::shared_ptr<IUsbDevice> usbDev, std::shared_ptr<const USBSourcePortInfo> portInfo)
    : usbDev_(usbDev), portInfo_(portInfo) {
    auto libusbDevice = std::dynamic_pointer_cast<UsbDeviceLibusb>(usbDev_);
    if(!libusbDevice) {
        THROW_INVALID_PARAM_EXCEPTION("ObLibUacDevicePort requires a libusb-backed device");
    }
    devHandle_ = libusbDevice->getLibusbDeviceHandle();
    libusbCtx_ = libusbDevice->getLibusbContext();

    parseUacTopology();

    // Detach the kernel snd-usb-audio driver before claiming the AudioStreaming interface.
    if(libusb_kernel_driver_active(devHandle_, asInterfaceNumber_) == 1) {
        auto res = libusb_detach_kernel_driver(devHandle_, asInterfaceNumber_);
        if(res != LIBUSB_SUCCESS) {
            THROW_IO_EXCEPTION("UAC: detach kernel driver failed, error: " + std::string(libusb_strerror(res)));
        }
        kernelDetached_ = true;
    }

    auto res = libusb_claim_interface(devHandle_, asInterfaceNumber_);
    if(res == LIBUSB_ERROR_BUSY) {
        LOG_WARN("UAC: libusb_claim_interface busy, retrying once. Interface: {}", asInterfaceNumber_);
        res = libusb_claim_interface(devHandle_, asInterfaceNumber_);
    }
    if(res != LIBUSB_SUCCESS) {
        THROW_IO_EXCEPTION("UAC: claim interface failed, error: " + std::string(libusb_strerror(res)));
    }

    LOG_DEBUG("ObLibUacDevicePort created: AS iface={}, alt={}, ep=0x{:02x}, maxPkt={}, UAC v{:#06x}, format={}Hz/{}ch/{}bit", asInterfaceNumber_,
              asAltSetting_, isoEndpointAddr_, isoMaxPacketSize_, uacBcdVersion_, sampleRate_, channelCount_, bitsPerSample_);
}

ObLibUacDevicePort::~ObLibUacDevicePort() noexcept {
    TRY_EXECUTE(stopAllStream());

    libusb_release_interface(devHandle_, asInterfaceNumber_);
    if(kernelDetached_) {
        if(libusb_kernel_driver_active(devHandle_, asInterfaceNumber_) == 0) {
            libusb_attach_kernel_driver(devHandle_, asInterfaceNumber_);
        }
    }
    LOG_DEBUG("ObLibUacDevicePort destroyed");
}

void ObLibUacDevicePort::parseUacTopology() {
    libusb_device            *device = libusb_get_device(devHandle_);
    libusb_config_descriptor *config = nullptr;
    auto                      ret    = libusb_get_active_config_descriptor(device, &config);
    if(ret != LIBUSB_SUCCESS || config == nullptr) {
        THROW_IO_EXCEPTION("UAC: failed to read active config descriptor: " + std::string(libusb_strerror(ret)));
    }

    bool foundStreaming = false;
    for(uint8_t i = 0; i < config->bNumInterfaces; ++i) {
        const auto &interface = config->interface[i];
        for(int a = 0; a < interface.num_altsetting; ++a) {
            const libusb_interface_descriptor &alt = interface.altsetting[a];
            if(alt.bInterfaceClass != LIBUSB_CLASS_AUDIO) {
                continue;
            }

            if(alt.bInterfaceSubClass == UAC_SUBCLASS_AUDIOCONTROL) {
                acInterfaceNumber_ = alt.bInterfaceNumber;
                // Best-effort parse of the class-specific AC descriptors for Feature Unit / Clock Source IDs.
                const uint8_t *extra    = alt.extra;
                int            extraLen = alt.extra_length;
                int            offset   = 0;
                while(offset + 3 <= extraLen) {
                    uint8_t bLength      = extra[offset];
                    uint8_t bDescType    = extra[offset + 1];
                    uint8_t bDescSubType = extra[offset + 2];
                    if(bLength == 0) {
                        break;
                    }
                    if(bDescType == UAC_CS_INTERFACE) {
                        if(bDescSubType == UAC_AC_HEADER && offset + 5 <= extraLen) {
                            uacBcdVersion_ = static_cast<uint16_t>(extra[offset + 3] | (extra[offset + 4] << 8));  // bcdADC
                        }
                        else if(bDescSubType == UAC_AC_FEATURE_UNIT && offset + 4 <= extraLen && featureUnitId_ == 0) {
                            featureUnitId_ = extra[offset + 3];  // bUnitID
                        }
                        else if(bDescSubType == UAC_AC_CLOCK_SOURCE && offset + 4 <= extraLen && clockSourceId_ == 0) {
                            clockSourceId_ = extra[offset + 3];  // bClockID
                        }
                    }
                    offset += bLength;
                }
            }
            else if(alt.bInterfaceSubClass == UAC_SUBCLASS_AUDIOSTREAMING && alt.bNumEndpoints > 0) {
                // alt setting >= 1 carrying the iso IN endpoint
                for(uint8_t e = 0; e < alt.bNumEndpoints; ++e) {
                    const libusb_endpoint_descriptor &ep    = alt.endpoint[e];
                    bool                              isIn  = (ep.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN;
                    bool                              isIso = (ep.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
                    if(isIn && isIso) {
                        asInterfaceNumber_ = alt.bInterfaceNumber;
                        asAltSetting_      = alt.bAlternateSetting;
                        isoEndpointAddr_   = ep.bEndpointAddress;
                        isoMaxPacketSize_  = ep.wMaxPacketSize;
                        foundStreaming     = true;

                        // Parse the class-specific AS Type I FORMAT_TYPE descriptor for the real audio format,
                        // instead of assuming a fixed 16 kHz mono. Each sample frequency is 3-byte little-endian.
                        // UAC1 only: the UAC2 FORMAT_TYPE has a different layout (rates live in the Clock Source),
                        // so leave the defaults there. bcdADC was parsed from the AC interface enumerated earlier.
                        const uint8_t *fx    = alt.extra;
                        int            fxLen = (uacBcdVersion_ < 0x0200) ? alt.extra_length : 0;
                        int            fxOff = 0;
                        while(fxOff + 3 <= fxLen) {
                            uint8_t bLength  = fx[fxOff];
                            uint8_t bType    = fx[fxOff + 1];
                            uint8_t bSubType = fx[fxOff + 2];
                            if(bLength == 0) {
                                break;
                            }
                            if(bType == UAC_CS_INTERFACE && bSubType == UAC_AS_FORMAT_TYPE && fxOff + 8 <= fxLen) {
                                uint8_t bNrChannels    = fx[fxOff + 4];
                                uint8_t bSubframeSize  = fx[fxOff + 5];  // bytes per sample
                                uint8_t bBitResolution = fx[fxOff + 6];
                                uint8_t bSamFreqType   = fx[fxOff + 7];
                                if(bNrChannels > 0) {
                                    channelCount_ = bNrChannels;
                                }
                                if(bBitResolution > 0) {
                                    bitsPerSample_ = bBitResolution;
                                }
                                else if(bSubframeSize > 0) {
                                    bitsPerSample_ = static_cast<uint8_t>(bSubframeSize * 8);
                                }
                                auto readFreq = [&](int p) -> uint32_t { return static_cast<uint32_t>(fx[p] | (fx[p + 1] << 8) | (fx[p + 2] << 16)); };
                                if(bSamFreqType == 0 && fxOff + 14 <= fxLen) {
                                    // Continuous range [lower, upper]; use the upper bound.
                                    sampleRate_ = readFreq(fxOff + 11);
                                }
                                else if(bSamFreqType > 0) {
                                    // Discrete list; prefer 48 kHz if offered, else the highest listed rate.
                                    uint32_t best = 0;
                                    for(int k = 0; k < bSamFreqType; ++k) {
                                        int p = fxOff + 8 + k * 3;
                                        if(p + 3 > fxLen) {
                                            break;
                                        }
                                        uint32_t f = readFreq(p);
                                        if(f == 48000) {
                                            best = f;
                                            break;
                                        }
                                        if(f > best) {
                                            best = f;
                                        }
                                    }
                                    if(best > 0) {
                                        sampleRate_ = best;
                                    }
                                }
                            }
                            fxOff += bLength;
                        }
                        break;
                    }
                }
            }
        }
    }

    libusb_free_config_descriptor(config);

    if(!foundStreaming) {
        THROW_IO_EXCEPTION("UAC: no AudioStreaming iso IN endpoint found in config descriptor");
    }
    if(featureUnitId_ == 0) {
        featureUnitId_ = 2;  // common default; should be confirmed against firmware
    }
}

StreamProfileList ObLibUacDevicePort::getStreamProfileList() {
    // Format parsed from the AS Type I FORMAT_TYPE descriptor (see parseUacTopology); falls back to 16 kHz mono.
    StreamProfileList list;
    uint32_t          samplesPerFrame = sampleRate_ / 50;  // ~20 ms chunk hint for buffer sizing
    list.push_back(StreamProfileFactory::createAudioStreamProfile(OB_FORMAT_PCM, sampleRate_, channelCount_, bitsPerSample_, 0, samplesPerFrame));
    return list;
}

bool ObLibUacDevicePort::setSamplingFrequency(uint32_t sampleRate) {
    uint16_t wValue = static_cast<uint16_t>(UAC_SAMPLING_FREQ_CONTROL << 8);  // selector in high byte, channel 0
    if(uacBcdVersion_ >= 0x0200) {
        // UAC2: SET_CUR of the Clock Source's Sampling Frequency Control, 4-byte value, addressed to the AC interface.
        uint8_t data[4] = { static_cast<uint8_t>(sampleRate), static_cast<uint8_t>(sampleRate >> 8), static_cast<uint8_t>(sampleRate >> 16),
                            static_cast<uint8_t>(sampleRate >> 24) };
        uint8_t bmRequestType =
            static_cast<uint8_t>(LIBUSB_ENDPOINT_OUT) | static_cast<uint8_t>(LIBUSB_REQUEST_TYPE_CLASS) | static_cast<uint8_t>(LIBUSB_RECIPIENT_INTERFACE);
        uint16_t wIndex = static_cast<uint16_t>((clockSourceId_ << 8) | acInterfaceNumber_);
        auto     res    = libusb_control_transfer(devHandle_, bmRequestType, UAC_SET_CUR, wValue, wIndex, data, sizeof(data), 1000);
        return res >= 0;
    }
    // UAC1: SET_CUR of the endpoint's Sampling Frequency Control, 3-byte value, addressed to the iso endpoint.
    uint8_t data[3] = { static_cast<uint8_t>(sampleRate), static_cast<uint8_t>(sampleRate >> 8), static_cast<uint8_t>(sampleRate >> 16) };
    uint8_t bmRequestType =
        static_cast<uint8_t>(LIBUSB_ENDPOINT_OUT) | static_cast<uint8_t>(LIBUSB_REQUEST_TYPE_CLASS) | static_cast<uint8_t>(LIBUSB_RECIPIENT_ENDPOINT);
    uint16_t wIndex = isoEndpointAddr_;
    auto     res    = libusb_control_transfer(devHandle_, bmRequestType, UAC_SET_CUR, wValue, wIndex, data, sizeof(data), 1000);
    return res >= 0;
}

void ObLibUacDevicePort::startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    std::lock_guard<std::mutex> lk(streamMutex_);
    if(streaming_) {
        THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("ObLibUacDevicePort::startStream() called while streaming");
    }
    activatedProfile_ = profile->as<AudioStreamProfile>();
    frameCallback_    = callback;
    anchorSteadyUs_   = 0;
    totalSamples_     = 0;

    auto res = libusb_set_interface_alt_setting(devHandle_, asInterfaceNumber_, asAltSetting_);
    if(res != LIBUSB_SUCCESS) {
        THROW_IO_EXCEPTION("UAC: set_interface_alt_setting failed: " + std::string(libusb_strerror(res)));
    }

    // Select the sampling frequency now that the streaming alt setting is active. A multi-rate device leaves
    // the iso IN endpoint idle (no frames) until told which rate to use; the kernel snd-usb-audio driver does
    // this implicitly, so skipping it is why direct libusb capture produced no audio frames.
    if(!setSamplingFrequency(activatedProfile_->getSampleRate())) {
        LOG_WARN("UAC: failed to set sampling frequency {} Hz; device may not deliver audio frames", activatedProfile_->getSampleRate());
    }

    streaming_ = true;

    transfers_.resize(kNumTransfers, nullptr);
    transferBuffers_.resize(kNumTransfers);
    const int bufLen = isoMaxPacketSize_ * kNumIsoPackets;
    for(int i = 0; i < kNumTransfers; ++i) {
        transferBuffers_[i].assign(static_cast<size_t>(bufLen), 0);
        auto transfer = libusb_alloc_transfer(kNumIsoPackets);
        if(!transfer) {
            streaming_ = false;
            THROW_MEMORY_EXCEPTION("UAC: libusb_alloc_transfer failed");
        }
        libusb_fill_iso_transfer(transfer, devHandle_, isoEndpointAddr_, transferBuffers_[i].data(), bufLen, kNumIsoPackets, &ObLibUacDevicePort::onIsoTransfer,
                                 this, 1000);
        libusb_set_iso_packet_lengths(transfer, isoMaxPacketSize_);
        transfers_[i] = transfer;
    }

    for(int i = 0; i < kNumTransfers; ++i) {
        auto sres = libusb_submit_transfer(transfers_[i]);
        if(sres != LIBUSB_SUCCESS) {
            LOG_WARN("UAC: submit iso transfer {} failed: {}", i, libusb_strerror(sres));
        }
    }

    eventThread_ = std::thread(&ObLibUacDevicePort::eventLoop, this);
    LOG_DEBUG("ObLibUacDevicePort streaming started");
}

void ObLibUacDevicePort::eventLoop() {
    while(streaming_) {
        struct timeval tv {
            0, 100000
        };  // 100 ms
        libusb_handle_events_timeout_completed(libusbCtx_, &tv, nullptr);
    }
}

void LIBUSB_CALL ObLibUacDevicePort::onIsoTransfer(struct libusb_transfer *transfer) {
    auto *self = static_cast<ObLibUacDevicePort *>(transfer->user_data);
    self->handleIsoTransfer(transfer);
}

void ObLibUacDevicePort::handleIsoTransfer(struct libusb_transfer *transfer) {
    if(!streaming_) {
        return;
    }

    if(transfer->status == LIBUSB_TRANSFER_COMPLETED || transfer->status == LIBUSB_TRANSFER_TIMED_OUT) {
        // Gather all valid iso packets of this transfer into one contiguous PCM chunk.
        std::vector<uint8_t> chunk;
        chunk.reserve(static_cast<size_t>(transfer->length));
        for(int p = 0; p < transfer->num_iso_packets; ++p) {
            const libusb_iso_packet_descriptor &pkt = transfer->iso_packet_desc[p];
            if(pkt.status == LIBUSB_TRANSFER_COMPLETED && pkt.actual_length > 0) {
                const uint8_t *data = libusb_get_iso_packet_buffer_simple(transfer, p);
                chunk.insert(chunk.end(), data, data + pkt.actual_length);
            }
        }

        if(!chunk.empty() && frameCallback_ && activatedProfile_) {
            auto frame = FrameFactory::createFrame(OB_FRAME_AUDIO, OB_FORMAT_PCM, chunk.size());
            frame->updateData(chunk.data(), chunk.size());

            uint32_t channelCount   = activatedProfile_->getChannelCount();
            uint32_t bytesPerSample = (activatedProfile_->getBitsPerSample() + 7) / 8;
            uint32_t sampleRate     = activatedProfile_->getSampleRate();
            uint32_t bytesPerFrame  = channelCount * bytesPerSample;
            uint32_t sampleCount    = bytesPerFrame ? static_cast<uint32_t>(chunk.size() / bytesPerFrame) : 0;

            auto nowUs    = utils::getNowTimesUs();
            auto steadyUs = utils::getSteadyTimeUs();
            if(anchorSteadyUs_ == 0) {
                anchorSteadyUs_ = steadyUs;
            }
            // First-sample timestamp: anchor + accumulated samples (equal-spaced axis, plan §5.4).
            uint64_t tsUs = sampleRate ? anchorSteadyUs_ + (totalSamples_.load() * 1000000ull) / sampleRate : steadyUs;
            frame->setTimeStampUsec(tsUs);
            frame->setSystemTimeStampUsec(nowUs);
            frame->setSteadyTimeStampUsec(steadyUs);
            totalSamples_.fetch_add(sampleCount);

            frameCallback_(frame);
        }
    }
    else if(transfer->status != LIBUSB_TRANSFER_CANCELLED) {
        LOG_WARN_INTVL(utils::string::to_string() << "UAC: iso transfer status=" << transfer->status);
    }

    if(streaming_) {
        auto res = libusb_submit_transfer(transfer);
        if(res != LIBUSB_SUCCESS && res != LIBUSB_ERROR_NO_DEVICE) {
            LOG_WARN_INTVL(utils::string::to_string() << "UAC: resubmit iso transfer failed: " << libusb_strerror(res));
        }
    }
}

void ObLibUacDevicePort::stopStream(std::shared_ptr<const StreamProfile> profile) {
    utils::unusedVar(profile);
    stopAllStream();
}

void ObLibUacDevicePort::stopAllStream() {
    std::lock_guard<std::mutex> lk(streamMutex_);
    if(!streaming_) {
        return;
    }
    streaming_ = false;

    for(auto *transfer: transfers_) {
        if(transfer) {
            libusb_cancel_transfer(transfer);
        }
    }

    if(eventThread_.joinable()) {
        eventThread_.join();
    }

    for(auto *transfer: transfers_) {
        if(transfer) {
            libusb_free_transfer(transfer);
        }
    }
    transfers_.clear();
    transferBuffers_.clear();

    // Switch back to the zero-bandwidth alt setting to release bus bandwidth.
    libusb_set_interface_alt_setting(devHandle_, asInterfaceNumber_, 0);

    frameCallback_    = nullptr;
    activatedProfile_ = nullptr;
    LOG_DEBUG("ObLibUacDevicePort streaming stopped");
}

uint32_t ObLibUacDevicePort::sendAndReceive(const uint8_t *sendData, uint32_t sendLen, uint8_t *recvData, uint32_t exceptedRecvLen,
                                            utils::TransferTiming *timing) {
    utils::unusedVar(sendData);
    utils::unusedVar(sendLen);
    utils::unusedVar(recvData);
    utils::unusedVar(exceptedRecvLen);
    utils::unusedVar(timing);
    return 0;
}

std::shared_ptr<const SourcePortInfo> ObLibUacDevicePort::getSourcePortInfo() const {
    return portInfo_;
}

}  // namespace libobsensor
