// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#ifdef _WIN32
#include "WasapiUacPlaybackPort.hpp"

#include "exception/ObException.hpp"

#include <Windows.h>
#include <audioclient.h>
#include <devicetopology.h>
// functiondiscoverykeys_devpkey.h declares PKEY_Device_* with
// DEFINE_PROPERTYKEY, which is provided by propkeydef.h but is not included
// by every Windows SDK version through the audio headers.
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <propidl.h>

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <string>
#include <utility>

namespace libobsensor {
namespace {
std::wstring upper(std::wstring value) {
    for(auto &ch: value) ch = static_cast<wchar_t>(std::towupper(ch));
    return value;
}

std::wstring makeVidPidToken(uint16_t vid, uint16_t pid) {
    wchar_t token[32] = {};
    swprintf(token, sizeof(token) / sizeof(token[0]), L"VID_%04X&PID_%04X", vid, pid);
    return token;
}

std::wstring makeCompactVidPidToken(uint16_t vid, uint16_t pid) {
    wchar_t token[16] = {};
    swprintf(token, sizeof(token) / sizeof(token[0]), L"%04X%04X", vid, pid);
    return token;
}

std::wstring getEndpointHardwareDeviceId(IMMDevice *endpoint) {
    std::wstring result;
    IDeviceTopology *topology = nullptr;
    if(SUCCEEDED(endpoint->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&topology))) && topology) {
        IConnector *connector = nullptr;
        if(SUCCEEDED(topology->GetConnector(0, &connector)) && connector) {
            IConnector *connected = nullptr;
            if(SUCCEEDED(connector->GetConnectedTo(&connected)) && connected) {
                IPart *part = nullptr;
                if(SUCCEEDED(connected->QueryInterface(__uuidof(IPart), reinterpret_cast<void **>(&part))) && part) {
                    IDeviceTopology *adapter = nullptr;
                    if(SUCCEEDED(part->GetTopologyObject(&adapter)) && adapter) {
                        LPWSTR id = nullptr;
                        if(SUCCEEDED(adapter->GetDeviceId(&id)) && id) {
                            result = id;
                            CoTaskMemFree(id);
                        }
                        adapter->Release();
                    }
                    part->Release();
                }
                connected->Release();
            }
            connector->Release();
        }
        topology->Release();
    }
    return result;
}
}  // namespace

WasapiUacPlaybackPort::WasapiUacPlaybackPort(std::shared_ptr<const USBSourcePortInfo> info) : info_(std::move(info)) {}
WasapiUacPlaybackPort::~WasapiUacPlaybackPort() noexcept { closeSession(); }
std::shared_ptr<const SourcePortInfo> WasapiUacPlaybackPort::getSourcePortInfo() const { return info_; }

void WasapiUacPlaybackPort::openSession() {
    if(sessionOpen_) return;
    const auto coResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    comInitialized_ = SUCCEEDED(coResult);
    if(FAILED(coResult) && coResult != RPC_E_CHANGED_MODE) THROW_IO_EXCEPTION("WASAPI playback: COM initialization failed");
    IMMDeviceEnumerator *enumerator = nullptr;
    IMMDeviceCollection *collection = nullptr;
    if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void **>(&enumerator)))
       || FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection))) {
        if(collection) collection->Release();
        if(enumerator) enumerator->Release();
        if(comInitialized_) CoUninitialize();
        comInitialized_ = false;
        THROW_IO_EXCEPTION("WASAPI playback: render endpoint enumeration failed");
    }
    const auto identityUpper = upper(makeVidPidToken(info_->vid, info_->pid));
    const auto compactIdentityUpper = upper(makeCompactVidPidToken(info_->vid, info_->pid));
    UINT count = 0;
    collection->GetCount(&count);
    for(UINT index = 0; index < count && !device_; ++index) {
        IMMDevice *candidate = nullptr;
        IPropertyStore *store = nullptr;
        PROPVARIANT value;
        PropVariantInit(&value);
        std::wstring instanceId;
        std::wstring parentId;
        if(SUCCEEDED(collection->Item(index, &candidate)) && candidate && SUCCEEDED(candidate->OpenPropertyStore(STGM_READ, &store)) && store) {
            if(SUCCEEDED(store->GetValue(PKEY_Device_InstanceId, &value)) && value.vt == VT_LPWSTR && value.pwszVal) instanceId = upper(value.pwszVal);
            PropVariantClear(&value);
            PropVariantInit(&value);
            if(SUCCEEDED(store->GetValue(PKEY_Device_Parent, &value)) && value.vt == VT_LPWSTR && value.pwszVal) parentId = upper(value.pwszVal);
        }
        if(candidate) {
            const auto hardwareId = upper(getEndpointHardwareDeviceId(candidate));
            if(instanceId.find(identityUpper) != std::wstring::npos || parentId.find(identityUpper) != std::wstring::npos
               || hardwareId.find(identityUpper) != std::wstring::npos || hardwareId.find(compactIdentityUpper) != std::wstring::npos) device_ = candidate;
        }
        if(candidate && candidate != device_) candidate->Release();
        if(store) store->Release();
        PropVariantClear(&value);
    }
    collection->Release();
    enumerator->Release();
    if(!device_) { if(comInitialized_) CoUninitialize(); comInitialized_ = false; THROW_ITEM_NOT_FOUND_EXCEPTION("EGO USB speaker endpoint not found"); }
    if(FAILED(device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&client_)))) {
        closeSession(); THROW_IO_EXCEPTION("WASAPI playback: activate audio client failed");
    }
    WAVEFORMATEX format = {};
    format.wFormatTag = WAVE_FORMAT_PCM; format.nChannels = 1; format.nSamplesPerSec = 16000; format.wBitsPerSample = 16;
    format.nBlockAlign = 2; format.nAvgBytesPerSec = 32000;
    if(FAILED(client_->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                  200000, 0, &format, nullptr))
       || FAILED(client_->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void **>(&render_)))) {
        closeSession(); THROW_IO_EXCEPTION("WASAPI playback: initialize 16 kHz mono stream failed");
    }
    sessionOpen_ = true;
    thread_ = std::thread(&WasapiUacPlaybackPort::renderLoop, this);
}

void WasapiUacPlaybackPort::beginPlayback(uint64_t generation) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!sessionOpen_) THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("WASAPI playback session is not open");
    if(playing_) THROW_RESOURCE_BUSY_EXCEPTION("WASAPI playback is already active");
    generation_ = generation; queue_.clear(); playing_ = true;
    if(FAILED(client_->Start())) { playing_ = false; THROW_IO_EXCEPTION("WASAPI playback: start failed"); }
}
size_t WasapiUacPlaybackPort::writePcm(const uint8_t *data, size_t size, uint64_t generation) {
    std::unique_lock<std::mutex> lock(mutex_);
    queueCv_.wait(lock, [&] { return !playing_ || generation_ != generation || queue_.size() + size <= kMaxQueueBytes; });
    if(!playing_ || generation_ != generation) return 0;
    queue_.insert(queue_.end(), data, data + size); return size;
}
bool WasapiUacPlaybackPort::drain(uint64_t generation) {
    std::unique_lock<std::mutex> lock(mutex_);
    drainCv_.wait(lock, [&] {
        if(!playing_ || generation_ != generation) return true;
        UINT32 padding = 0;
        return queue_.empty() && SUCCEEDED(client_->GetCurrentPadding(&padding)) && padding == 0;
    });
    return playing_ && generation_ == generation;
}
void WasapiUacPlaybackPort::endPlayback(uint64_t generation) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if(generation_ != generation || !playing_) return;
    playing_ = false; queue_.clear(); queueCv_.notify_all(); drainCv_.notify_all();
    // stop() reaches this path before drain; Reset discards any buffered PCM immediately.
    client_->Stop(); client_->Reset();
}
void WasapiUacPlaybackPort::closeSession() noexcept {
    endPlayback(generation_); sessionOpen_ = false; queueCv_.notify_all(); drainCv_.notify_all();
    if(thread_.joinable()) thread_.join();
    if(render_) { render_->Release(); render_ = nullptr; }
    if(client_) { client_->Release(); client_ = nullptr; }
    if(device_) { device_->Release(); device_ = nullptr; }
    if(comInitialized_) CoUninitialize();
    comInitialized_ = false;
}
void WasapiUacPlaybackPort::renderLoop() {
    const bool comInitialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    while(sessionOpen_) {
        std::unique_lock<std::mutex> lock(mutex_);
        if(!playing_) { lock.unlock(); std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue; }
        UINT32 padding = 0, capacity = 0;
        if(FAILED(client_->GetCurrentPadding(&padding)) || FAILED(client_->GetBufferSize(&capacity))) {
            playing_ = false;
            queue_.clear();
            queueCv_.notify_all();
            drainCv_.notify_all();
            continue;
        }
        const UINT32 frames = capacity - padding;
        if(!frames || queue_.empty()) { drainCv_.notify_all(); lock.unlock(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); continue; }
        const auto bytes = std::min<size_t>(queue_.size(), static_cast<size_t>(frames) * 2);
        const auto writeFrames = static_cast<UINT32>(bytes / 2);
        BYTE *buffer = nullptr;
        if(FAILED(render_->GetBuffer(writeFrames, &buffer))) {
            playing_ = false;
            queue_.clear();
            queueCv_.notify_all();
            drainCv_.notify_all();
            continue;
        }
        for(size_t i = 0; i < bytes; ++i) { buffer[i] = queue_.front(); queue_.pop_front(); }
        render_->ReleaseBuffer(writeFrames, 0);
        queueCv_.notify_all(); drainCv_.notify_all();
    }
    if(comInitialized) CoUninitialize();
}
}  // namespace libobsensor
#endif
