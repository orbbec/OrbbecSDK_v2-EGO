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
#include <functiondiscoverykeys_devpkey.h>

#include <algorithm>
#include <cwctype>
#include <string>

namespace libobsensor {

namespace {
// Build the "VID_xxxx&PID_yyyy" token (uppercase hex) used inside PKEY_Device_InstanceId.
std::wstring makeVidPidToken(uint16_t vid, uint16_t pid) {
    wchar_t buf[32];
    swprintf(buf, 32, L"VID_%04X&PID_%04X", vid, pid);
    return std::wstring(buf);
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
        auto token = makeVidPidToken(portInfo_->vid, portInfo_->pid);

        // [EGO-SIM] TEMPORARY (remove with EGO_SIMULATE_WITH_FEMTO_MEGA): friendly-name fallback match.
        // Only the simulated EGO injects an infName hint (external mic stands in for the missing Femto Mega
        // microphone). Windows audio endpoints often expose a SWD\MMDEVAPI instance id that does not carry
        // the USB VID/PID, so also allow matching by endpoint friendly-name substring (carried in infName).
        // Real EGO hardware reports a proper UAC VID/PID, so the VID/PID match above suffices then.
        std::wstring nameHint(portInfo_->infName.begin(), portInfo_->infName.end());
        std::transform(nameHint.begin(), nameHint.end(), nameHint.begin(), ::towupper);

        for(UINT i = 0; i < count && mmDevice_ == nullptr; ++i) {
            IMMDevice *dev = nullptr;
            if(FAILED(collection->Item(i, &dev)) || !dev) {
                continue;
            }
            IPropertyStore *props = nullptr;
            if(SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props)) && props) {
                bool matched = false;

                PROPVARIANT idVar;
                PropVariantInit(&idVar);
                if(SUCCEEDED(props->GetValue(PKEY_Device_InstanceId, &idVar)) && idVar.vt == VT_LPWSTR && idVar.pwszVal) {
                    std::wstring instanceId(idVar.pwszVal);
                    std::transform(instanceId.begin(), instanceId.end(), instanceId.begin(), ::towupper);
                    matched = instanceId.find(token) != std::wstring::npos;
                }
                PropVariantClear(&idVar);

                if(!matched && !nameHint.empty()) {  // [EGO-SIM] friendly-name fallback; remove with simulation
                    PROPVARIANT nameVar;
                    PropVariantInit(&nameVar);
                    if(SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &nameVar)) && nameVar.vt == VT_LPWSTR && nameVar.pwszVal) {
                        std::wstring friendly(nameVar.pwszVal);
                        std::transform(friendly.begin(), friendly.end(), friendly.begin(), ::towupper);
                        matched = friendly.find(nameHint) != std::wstring::npos;
                    }
                    PropVariantClear(&nameVar);
                }

                if(matched) {
                    mmDevice_ = dev;
                    dev       = nullptr;  // ownership moved
                }
                props->Release();
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

    // Read the endpoint mix format to populate the reported audio profile.
    IAudioClient *client = nullptr;
    if(SUCCEEDED(mmDevice_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&client))) && client) {
        WAVEFORMATEX *wfx = nullptr;
        if(SUCCEEDED(client->GetMixFormat(&wfx)) && wfx) {
            sampleRate_    = wfx->nSamplesPerSec;
            channelCount_  = wfx->nChannels;
            bitsPerSample_ = wfx->wBitsPerSample;
            CoTaskMemFree(wfx);
        }
        client->Release();
    }

    LOG_DEBUG("WasapiUacDevicePort created: sampleRate={}, channels={}, bits={}", sampleRate_, channelCount_, bitsPerSample_);
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
    // samplesPerFrame is a soft hint (~20 ms chunk) for buffer sizing; WASAPI delivers variable packet sizes.
    uint32_t samplesPerFrame = sampleRate_ / 50;
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
    anchorQpcUs_      = 0;
    totalFrames_      = 0;

    HRESULT hr = mmDevice_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&audioClient_));
    if(FAILED(hr) || !audioClient_) {
        THROW_IO_EXCEPTION("WASAPI: failed to activate IAudioClient");
    }

    WAVEFORMATEX *wfx = nullptr;
    audioClient_->GetMixFormat(&wfx);

    REFERENCE_TIME bufferDuration = 200000;  // 20 ms in 100-ns units
    hr                            = audioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDuration, 0, wfx, nullptr);
    if(wfx) {
        CoTaskMemFree(wfx);
    }
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

                // qpcPos is in 100-ns units; anchor on the first packet, then advance by sample count.
                uint64_t qpcUs = qpcPos / 10;
                if(anchorQpcUs_ == 0) {
                    anchorQpcUs_ = qpcUs;
                }
                uint64_t tsUs = sampleRate_ ? anchorQpcUs_ + (totalFrames_ * 1000000ull) / sampleRate_ : qpcUs;
                frame->setTimeStampUsec(tsUs);
                frame->setSystemTimeStampUsec(utils::getNowTimesUs());
                frame->setSteadyTimeStampUsec(utils::getSteadyTimeUs());
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
