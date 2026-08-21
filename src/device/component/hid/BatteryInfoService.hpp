// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceComponent.hpp"
#include "ISourcePort.hpp"
#include "libobsensor/h/ObTypes.h"

#include <functional>
#include <mutex>

namespace libobsensor {

class BatteryInfoService : public IDeviceComponent {
public:
    using BatteryInfoCallback = std::function<void(const OBBatteryInfo &)>;

    BatteryInfoService(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend);
    ~BatteryInfoService() noexcept override;

    void     setCallback(BatteryInfoCallback callback);
    void     clearCallback();
    IDevice *getOwner() const override;

private:
    void onFrame(std::shared_ptr<Frame> frame);

private:
    IDevice                        *owner_;
    std::shared_ptr<IDataStreamPort> backend_;

    std::mutex          mutex_;
    std::mutex          backendMutex_;
    BatteryInfoCallback callback_;
    bool                running_ = false;
};

}  // namespace libobsensor
