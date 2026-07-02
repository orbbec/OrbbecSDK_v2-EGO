// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceComponent.hpp"
#include "ISourcePort.hpp"

#include <functional>
#include <mutex>

namespace libobsensor {

class DirectionKeyService : public IDeviceComponent {
public:
    using DirectionKeyCallback = std::function<void(const OBDirectionKeyInfomation &)>;

    DirectionKeyService(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend);
    ~DirectionKeyService() noexcept override;

    void     setCallback(DirectionKeyCallback callback);
    void     clearCallback();
    IDevice *getOwner() const override;

private:
    void onFrame(std::shared_ptr<Frame> frame);

private:
    IDevice                         *owner_;
    std::shared_ptr<IDataStreamPort> backend_;

    std::mutex           mutex_;
    DirectionKeyCallback callback_;
    bool                 running_ = false;
};

}  // namespace libobsensor
