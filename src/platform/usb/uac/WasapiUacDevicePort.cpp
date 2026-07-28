// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#ifdef _WIN32

#include "WasapiUacDevicePort.hpp"

#include "frame/FrameFactory.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <devicetopology.h>
#include <functiondiscoverykeys_devpkey.h>

#include <algorithm>
#include <cwctype>
#include <string>

namespace libobsensor {

namespace {
// Build the "VID_xxxx&PID_yyyy" token (uppercase hex) used inside the USB device id.
std::wstring makeVidPidToken(uint16_t vid, uint16_t pid) {
    wchar_t buf[32];
    swprintf(buf, 32, L"VID_%04X&PID_%04X", vid, pid);
    return std::wstring(buf);
}

// Some Windows audio stacks (for example IntelAudio USB topology endpoints) flatten the USB
// identifiers into a contiguous hex fingerprint such as "...2BC512010400..." instead of the
// canonical "VID_2BC5&PID_1201" string.
std::wstring makeCompactVidPidToken(uint16_t vid, uint16_t pid) {
    wchar_t buf[16];
    swprintf(buf, 16, L"%04X%04X", vid, pid);
    return std::wstring(buf);
}

std::wstring toUpper(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), ::towupper);
    return s;
}

std::wstring guidToString(REFGUID guid) {
    wchar_t buf[64] = {};
    if(StringFromGUID2(guid, buf, static_cast<int>(sizeof(buf) / sizeof(buf[0]))) <= 0) {
        return std::wstring();
    }
    return std::wstring(buf);
}

// Convert a wide string to UTF-8 for logging.
std::string narrow(const std::wstring &s) {
    if(s.empty()) {
        return std::string();
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if(size <= 0) {
        return std::string();
    }
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &out[0], size, nullptr, nullptr);
    return out;
}

// Resolve an audio endpoint to the underlying hardware device id by following the device-topology
// connector to the adapter (KS) device. For USB audio this id carries the USB VID/PID
// (e.g. "\\?\USB#VID_2BC5&PID_1201&MI_04#..."), which the endpoint's own PKEY_Device_InstanceId
// (an "SWD\MMDEVAPI\..." software id) does not. Returns an empty string on failure.
std::wstring getEndpointHardwareDeviceId(IMMDevice *endpoint) {
    std::wstring     result;
    IDeviceTopology *topology = nullptr;
    if(SUCCEEDED(endpoint->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&topology))) && topology) {
        IConnector *connector = nullptr;
        if(SUCCEEDED(topology->GetConnector(0, &connector)) && connector) {
            IConnector *connectedTo = nullptr;
            if(SUCCEEDED(connector->GetConnectedTo(&connectedTo)) && connectedTo) {
                IPart *part = nullptr;
                if(SUCCEEDED(connectedTo->QueryInterface(__uuidof(IPart), reinterpret_cast<void **>(&part))) && part) {
                    IDeviceTopology *adapterTopology = nullptr;
                    if(SUCCEEDED(part->GetTopologyObject(&adapterTopology)) && adapterTopology) {
                        LPWSTR devId = nullptr;
                        if(SUCCEEDED(adapterTopology->GetDeviceId(&devId)) && devId) {
                            result = devId;
                            CoTaskMemFree(devId);
                        }
                        adapterTopology->Release();
                    }
                    part->Release();
                }
                connectedTo->Release();
            }
            connector->Release();
        }
        topology->Release();
    }
    return result;
}
}  // namespace

WasapiUacDevicePort::WasapiUacDevicePort(std::shared_ptr<const USBSourcePortInfo> portInfo) : portInfo_(portInfo) {
    HRESULT hr        = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool    comInited = SUCCEEDED(hr);

    IMMDeviceEnumerator *enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void **>(&enumerator));
    if(FAILED(hr) || !enumerator) {
        if(comInited) {
            CoUninitialize();
        }
        THROW_IO_EXCEPTION("WASAPI: failed to create device enumerator");
    }

    IMMDeviceCollection *collection = nullptr;
    hr                              = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
    if(SUCCEEDED(hr) && collection) {
        UINT count = 0;
        collection->GetCount(&count);
        auto token        = makeVidPidToken(portInfo_->vid, portInfo_->pid);
        auto compactToken = makeCompactVidPidToken(portInfo_->vid, portInfo_->pid);
        LOG_DEBUG("WASAPI: looking for capture endpoint matching {} / {}, {} active endpoint(s) found", narrow(token), narrow(compactToken), count);

        for(UINT i = 0; i < count && mmDevice_ == nullptr; ++i) {
            IMMDevice *dev = nullptr;
            if(FAILED(collection->Item(i, &dev)) || !dev) {
                continue;
            }

            std::wstring    instanceId;
            std::wstring    friendlyName;
            std::wstring    parentId;
            std::wstring    containerId;
            IPropertyStore *props = nullptr;
            if(SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props)) && props) {
                PROPVARIANT var;
                PropVariantInit(&var);
                if(SUCCEEDED(props->GetValue(PKEY_Device_InstanceId, &var)) && var.vt == VT_LPWSTR && var.pwszVal) {
                    instanceId = var.pwszVal;
                }
                PropVariantClear(&var);
                if(SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var)) && var.vt == VT_LPWSTR && var.pwszVal) {
                    friendlyName = var.pwszVal;
                }
                PropVariantClear(&var);
                if(SUCCEEDED(props->GetValue(PKEY_Device_Parent, &var)) && var.vt == VT_LPWSTR && var.pwszVal) {
                    parentId = var.pwszVal;
                }
                PropVariantClear(&var);
                if(SUCCEEDED(props->GetValue(PKEY_Device_ContainerId, &var))) {
                    if(var.vt == VT_CLSID && var.puuid) {
                        containerId = guidToString(*var.puuid);
                    }
                    else if(var.vt == VT_LPWSTR && var.pwszVal) {
                        containerId = var.pwszVal;
                    }
                }
                PropVariantClear(&var);
                props->Release();
            }

            // The endpoint's MMDEVAPI instance id ("SWD\MMDEVAPI\...") rarely carries the USB VID/PID, so also
            // resolve the underlying USB/KS device id via the device topology, which does. Some Windows builds
            // expose the underlying USB interface more reliably via DEVPKEY_Device_Parent, so consider that too.
            std::wstring hwId = getEndpointHardwareDeviceId(dev);

            LOG_DEBUG("WASAPI: endpoint[{}] name='{}' instanceId='{}' parentId='{}' containerId='{}' hwId='{}'", i, narrow(friendlyName), narrow(instanceId),
                      narrow(parentId), narrow(containerId), narrow(hwId));

            auto upperInstanceId = toUpper(instanceId);
            auto upperParentId   = toUpper(parentId);
            auto upperHwId       = toUpper(hwId);
            bool matched         = upperInstanceId.find(token) != std::wstring::npos || upperParentId.find(token) != std::wstring::npos
                           || upperHwId.find(token) != std::wstring::npos || upperHwId.find(compactToken) != std::wstring::npos;
            if(matched) {
                mmDevice_ = dev;
                dev       = nullptr;  // ownership moved
            }
            if(dev) {
                dev->Release();
            }
        }
        collection->Release();
    }
    enumerator->Release();

    if(!mmDevice_) {
        if(comInited) {
            CoUninitialize();
        }
        THROW_ITEM_NOT_FOUND_EXCEPTION("WASAPI: no matching UAC capture endpoint found for the EGO device");
    }

    // Force a fixed 16-bit integer PCM format (48 kHz mono) to match the EGO firmware / network path.
    // The WASAPI shared-mode mix format is typically 32-bit float @48 kHz; consuming that as OB_FORMAT_PCM
    // (16-bit) produced noise. We request the target format explicitly at stream start via
    // AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM, so the audio engine resamples/reformats for us. Keep the members
    // at the fixed target (header defaults 48000/1/16) instead of following the endpoint mix format.
    LOG_DEBUG("WasapiUacDevicePort created: sampleRate={}, channels={}, bits={} (forced 16-bit PCM)", sampleRate_, channelCount_, bitsPerSample_);
}

WasapiUacDevicePort::~WasapiUacDevicePort() noexcept {
    TRY_EXECUTE(stopAllStream());
    if(mmDevice_) {
        mmDevice_->Release();
        mmDevice_ = nullptr;
    }
    CoUninitialize();
    LOG_DEBUG("WasapiUacDevicePort destroyed");
}

StreamProfileList WasapiUacDevicePort::getStreamProfileList() {
    StreamProfileList list;
    // Match Ego firmware audio framing. WASAPI may still deliver variable packet sizes.
    uint32_t samplesPerFrame = 2048;
    list.push_back(StreamProfileFactory::createAudioStreamProfile(OB_FORMAT_PCM, sampleRate_, channelCount_, bitsPerSample_, 0, samplesPerFrame));
    return list;
}

void WasapiUacDevicePort::startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    std::lock_guard<std::mutex> lk(streamMutex_);
    if(streaming_) {
        THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("WasapiUacDevicePort::startStream() called while streaming");
    }
    activatedProfile_ = profile->as<AudioStreamProfile>();
    frameCallback_    = callback;
    fitter_.reset(sampleRate_);
    totalFrames_ = 0;

    HRESULT hr = mmDevice_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&audioClient_));
    if(FAILED(hr) || !audioClient_) {
        THROW_IO_EXCEPTION("WASAPI: failed to activate IAudioClient");
    }

    // Explicitly request 16-bit integer PCM at the fixed target rate/channels. In shared mode the audio
    // engine resamples/reformats the endpoint stream to this format when AUTOCONVERTPCM is set, so the
    // capture buffer already carries clean 16-bit PCM (no manual float->int conversion needed).
    WAVEFORMATEX wfxTarget    = {};
    wfxTarget.wFormatTag      = WAVE_FORMAT_PCM;
    wfxTarget.nChannels       = static_cast<WORD>(channelCount_);
    wfxTarget.nSamplesPerSec  = sampleRate_;
    wfxTarget.wBitsPerSample  = static_cast<WORD>(bitsPerSample_);
    wfxTarget.nBlockAlign     = static_cast<WORD>(wfxTarget.nChannels * wfxTarget.wBitsPerSample / 8);
    wfxTarget.nAvgBytesPerSec = wfxTarget.nSamplesPerSec * wfxTarget.nBlockAlign;
    wfxTarget.cbSize          = 0;

    REFERENCE_TIME bufferDuration = 200000;  // 20 ms in 100-ns units
    DWORD          streamFlags    = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    hr                            = audioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED, streamFlags, bufferDuration, 0, &wfxTarget, nullptr);
    if(FAILED(hr)) {
        audioClient_->Release();
        audioClient_ = nullptr;
        THROW_IO_EXCEPTION("WASAPI: failed to initialize IAudioClient");
    }

    hr = audioClient_->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void **>(&captureClient_));
    if(FAILED(hr) || !captureClient_) {
        audioClient_->Release();
        audioClient_ = nullptr;
        THROW_IO_EXCEPTION("WASAPI: failed to get IAudioCaptureClient");
    }

    hr = audioClient_->Start();
    if(FAILED(hr)) {
        captureClient_->Release();
        captureClient_ = nullptr;
        audioClient_->Release();
        audioClient_ = nullptr;
        THROW_IO_EXCEPTION("WASAPI: failed to start audio client");
    }

    streaming_     = true;
    captureThread_ = std::thread(&WasapiUacDevicePort::captureLoop, this);
    LOG_DEBUG("WasapiUacDevicePort streaming started");
}

void WasapiUacDevicePort::captureLoop() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    const uint32_t bytesPerSample = (bitsPerSample_ + 7) / 8;
    const uint32_t bytesPerFrame  = channelCount_ * bytesPerSample;

    while(streaming_) {
        UINT32 packetLength = 0;
        if(FAILED(captureClient_->GetNextPacketSize(&packetLength))) {
            break;
        }

        while(packetLength != 0 && streaming_) {
            BYTE   *data      = nullptr;
            UINT32  numFrames = 0;
            DWORD   flags     = 0;
            UINT64  devPos    = 0;
            UINT64  qpcPos    = 0;
            HRESULT hr        = captureClient_->GetBuffer(&data, &numFrames, &flags, &devPos, &qpcPos);
            if(FAILED(hr)) {
                break;
            }

            size_t byteCount = static_cast<size_t>(numFrames) * bytesPerFrame;
            if(byteCount > 0 && frameCallback_ && activatedProfile_) {
                auto frame = FrameFactory::createFrame(OB_FRAME_AUDIO, OB_FORMAT_PCM, byteCount);
                if((flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0 && data != nullptr) {
                    frame->updateData(reinterpret_cast<const uint8_t *>(data), byteCount);
                }

                uint64_t nowUs         = utils::getNowTimesUs();
                uint64_t pktDurationUs = (uint64_t)numFrames * 1000000ULL / sampleRate_;
                uint64_t tsUs          = fitter_.update(totalFrames_, nowUs - pktDurationUs);
                frame->setTimeStampUsec(tsUs);
                frame->setSystemTimeStampUsec(nowUs);
                frame->setSteadyTimeStampUsec(utils::getSteadyTimeUs());
                frame->setDeviceTimestampFromHost(true);
                totalFrames_ += numFrames;

                frameCallback_(frame);
            }

            captureClient_->ReleaseBuffer(numFrames);
            if(FAILED(captureClient_->GetNextPacketSize(&packetLength))) {
                break;
            }
        }

        // Sleep roughly half the buffer period to avoid busy-waiting.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CoUninitialize();
}

void WasapiUacDevicePort::stopStream(std::shared_ptr<const StreamProfile> profile) {
    utils::unusedVar(profile);
    stopAllStream();
}

void WasapiUacDevicePort::stopAllStream() {
    std::lock_guard<std::mutex> lk(streamMutex_);
    if(!streaming_) {
        return;
    }
    streaming_ = false;

    if(captureThread_.joinable()) {
        captureThread_.join();
    }

    if(audioClient_) {
        audioClient_->Stop();
    }
    if(captureClient_) {
        captureClient_->Release();
        captureClient_ = nullptr;
    }
    if(audioClient_) {
        audioClient_->Release();
        audioClient_ = nullptr;
    }

    frameCallback_    = nullptr;
    activatedProfile_ = nullptr;
    LOG_DEBUG("WasapiUacDevicePort streaming stopped");
}

std::shared_ptr<const SourcePortInfo> WasapiUacDevicePort::getSourcePortInfo() const {
    return portInfo_;
}

}  // namespace libobsensor

#endif  // _WIN32
