// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceComponent.hpp"
#include "ISourcePort.hpp"

#include <condition_variable>
#include <map>
#include <mutex>

namespace libobsensor {

class SharedDataStreamPort : public IDeviceComponent, public std::enable_shared_from_this<SharedDataStreamPort> {
public:
    SharedDataStreamPort(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend);
    ~SharedDataStreamPort() noexcept override;

    std::shared_ptr<IDataStreamPort> createClientPort();
    IDevice                         *getOwner() const override;

private:
    class ClientPort;

    enum class BackendState {
        Stopped,
        Starting,
        Running,
        Stopping,
    };

    struct ClientInfo {
        bool                 streaming = false;
        MutableFrameCallback callback;
    };

    void startClient(uint64_t clientId, MutableFrameCallback callback);
    void stopClient(uint64_t clientId, bool throwIfInactive);
    void releaseClient(uint64_t clientId) noexcept;
    void dispatchFrame(std::shared_ptr<Frame> frame);
    void waitForBackendStable(std::unique_lock<std::mutex> &lock);

private:
    IDevice                         *owner_;
    std::shared_ptr<IDataStreamPort> backend_;

    std::mutex              mutex_;
    std::condition_variable stateCv_;
    std::map<uint64_t, ClientInfo> clients_;
    BackendState            backendState_     = BackendState::Stopped;
    uint64_t                nextClientId_     = 1;
    uint32_t                activeClientCount_ = 0;
};

}  // namespace libobsensor
