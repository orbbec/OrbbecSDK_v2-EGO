// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "SharedDataStreamPort.hpp"

#include "exception/ObException.hpp"
#include "logger/Logger.hpp"

#include <vector>

namespace libobsensor {

class SharedDataStreamPort::ClientPort : public IDataStreamPort {
public:
    ClientPort(std::weak_ptr<SharedDataStreamPort> owner, uint64_t clientId) : owner_(std::move(owner)), clientId_(clientId) {}

    ~ClientPort() noexcept override {
        auto owner = owner_.lock();
        if(owner) {
            owner->releaseClient(clientId_);
        }
    }

    void startStream(MutableFrameCallback callback) override {
        auto owner = owner_.lock();
        if(!owner) {
            THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("SharedDataStreamPort client is no longer available");
        }
        owner->startClient(clientId_, callback);
    }

    void stopStream() override {
        auto owner = owner_.lock();
        if(!owner) {
            THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("SharedDataStreamPort client is no longer available");
        }
        owner->stopClient(clientId_, true);
    }

    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override {
        auto owner = owner_.lock();
        if(!owner) {
            return nullptr;
        }
        return owner->backend_->getSourcePortInfo();
    }

private:
    std::weak_ptr<SharedDataStreamPort> owner_;
    uint64_t                            clientId_;
};

SharedDataStreamPort::SharedDataStreamPort(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend) : owner_(owner), backend_(backend) {}

SharedDataStreamPort::~SharedDataStreamPort() noexcept {
    bool needStop = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        waitForBackendStable(lock);
        clients_.clear();
        activeClientCount_ = 0;
        needStop           = backendState_ == BackendState::Running;
        if(needStop) {
            backendState_ = BackendState::Stopping;
        }
    }

    if(needStop) {
        try {
            backend_->stopStream();
        }
        catch(const std::exception &e) {
            LOG_WARN("Failed to stop shared HID backend while destroying SharedDataStreamPort: {}", e.what());
        }
        catch(...) {
            LOG_WARN("Failed to stop shared HID backend while destroying SharedDataStreamPort");
        }

        std::lock_guard<std::mutex> lock(mutex_);
        backendState_ = BackendState::Stopped;
        stateCv_.notify_all();
    }
}

std::shared_ptr<IDataStreamPort> SharedDataStreamPort::createClientPort() {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto                  clientId = nextClientId_++;
    clients_.emplace(clientId, ClientInfo{});
    return std::make_shared<ClientPort>(shared_from_this(), clientId);
}

IDevice *SharedDataStreamPort::getOwner() const {
    return owner_;
}

void SharedDataStreamPort::startClient(uint64_t clientId, MutableFrameCallback callback) {
    bool needStart = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        waitForBackendStable(lock);

        auto iter = clients_.find(clientId);
        if(iter == clients_.end()) {
            THROW_ITEM_NOT_FOUND_EXCEPTION("SharedDataStreamPort client not found");
        }
        if(iter->second.streaming) {
            THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("SharedDataStreamPort client is already streaming");
        }

        iter->second.callback  = callback;
        iter->second.streaming = true;

        if(activeClientCount_ == 0 && backendState_ == BackendState::Stopped) {
            backendState_ = BackendState::Starting;
            needStart     = true;
        }
        activeClientCount_++;
    }

    if(!needStart) {
        return;
    }

    try {
        auto weakSelf = std::weak_ptr<SharedDataStreamPort>(shared_from_this());
        backend_->startStream([weakSelf](std::shared_ptr<Frame> frame) {
            auto self = weakSelf.lock();
            if(self) {
                self->dispatchFrame(std::move(frame));
            }
        });
    }
    catch(...) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto                        iter = clients_.find(clientId);
        if(iter != clients_.end() && iter->second.streaming) {
            iter->second.streaming = false;
            iter->second.callback  = nullptr;
            activeClientCount_--;
        }
        backendState_ = BackendState::Stopped;
        stateCv_.notify_all();
        throw;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    backendState_ = BackendState::Running;
    stateCv_.notify_all();
}

void SharedDataStreamPort::stopClient(uint64_t clientId, bool throwIfInactive) {
    bool needStop = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        waitForBackendStable(lock);

        auto iter = clients_.find(clientId);
        if(iter == clients_.end()) {
            if(throwIfInactive) {
                THROW_ITEM_NOT_FOUND_EXCEPTION("SharedDataStreamPort client not found");
            }
            return;
        }
        if(!iter->second.streaming) {
            if(throwIfInactive) {
                THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("SharedDataStreamPort client is not streaming");
            }
            return;
        }

        iter->second.streaming = false;
        iter->second.callback  = nullptr;
        activeClientCount_--;

        if(activeClientCount_ == 0 && backendState_ == BackendState::Running) {
            backendState_ = BackendState::Stopping;
            needStop      = true;
        }
    }

    if(!needStop) {
        return;
    }

    backend_->stopStream();

    std::lock_guard<std::mutex> lock(mutex_);
    backendState_ = BackendState::Stopped;
    stateCv_.notify_all();
}

void SharedDataStreamPort::releaseClient(uint64_t clientId) noexcept {
    bool needStop = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        waitForBackendStable(lock);

        auto iter = clients_.find(clientId);
        if(iter == clients_.end()) {
            return;
        }

        if(iter->second.streaming) {
            iter->second.streaming = false;
            iter->second.callback  = nullptr;
            if(activeClientCount_ > 0) {
                activeClientCount_--;
            }

            if(activeClientCount_ == 0 && backendState_ == BackendState::Running) {
                backendState_ = BackendState::Stopping;
                needStop      = true;
            }
        }

        clients_.erase(iter);
    }

    if(needStop) {
        try {
            backend_->stopStream();
            std::lock_guard<std::mutex> lock(mutex_);
            backendState_ = BackendState::Stopped;
        }
        catch(const std::exception &e) {
            LOG_WARN("Failed to stop shared HID backend while releasing client: {}", e.what());
            std::lock_guard<std::mutex> lock(mutex_);
            backendState_ = BackendState::Running;
        }
        catch(...) {
            LOG_WARN("Failed to stop shared HID backend while releasing client");
            std::lock_guard<std::mutex> lock(mutex_);
            backendState_ = BackendState::Running;
        }
        stateCv_.notify_all();
    }
}

void SharedDataStreamPort::dispatchFrame(std::shared_ptr<Frame> frame) {
    std::vector<MutableFrameCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks.reserve(activeClientCount_);
        for(const auto &client: clients_) {
            if(client.second.streaming && client.second.callback) {
                callbacks.emplace_back(client.second.callback);
            }
        }
    }

    for(auto &callback: callbacks) {
        callback(frame);
    }
}

void SharedDataStreamPort::waitForBackendStable(std::unique_lock<std::mutex> &lock) {
    stateCv_.wait(lock, [this]() { return backendState_ != BackendState::Starting && backendState_ != BackendState::Stopping; });
}

}  // namespace libobsensor
