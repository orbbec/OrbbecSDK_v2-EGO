// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceComponent.hpp"
#include "ISourcePort.hpp"
#include "libobsensor/h/ObTypes.h"

#include <functional>
#include <mutex>

namespace libobsensor {

class EgoStateService : public IDeviceComponent {
public:
    using EgoStateCallback = std::function<void(const OBEgoStateReport &)>;

    EgoStateService(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend);
    ~EgoStateService() noexcept override;

    void     setCallback(EgoStateCallback callback);
    void     clearCallback();
    IDevice *getOwner() const override;

private:
    void onFrame(std::shared_ptr<Frame> frame);

private:
    IDevice                         *owner_;
    std::shared_ptr<IDataStreamPort> backend_;

    std::mutex          mutex_;
    EgoStateCallback callback_;
    bool                running_ = false;
};

}  // namespace libobsensor
