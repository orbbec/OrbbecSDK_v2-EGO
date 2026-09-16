// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IFilter.hpp"
#include "ISourcePort.hpp"
#include "IDeviceComponent.hpp"
#include "hid/HidTypes.hpp"
#include "ImuCalculator.hpp"
#include "ImuPayloadDecoder.hpp"
#include "IStreamer.hpp"

#include <atomic>
#include <map>
#include <mutex>

namespace libobsensor {

class ImuStreamer : public IDeviceComponent, public IStreamer {
public:
    ImuStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend, const std::shared_ptr<IFilter> &filter,
                std::shared_ptr<IImuCalculator> calculator = nullptr);
    ImuStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend, std::vector<std::shared_ptr<IFilter>> filters,
                std::shared_ptr<IImuCalculator> calculator = nullptr);

    // Decoder-based construction: inject the payload decoder directly (e.g. DMC640Decoder) instead of a calculator.
    ImuStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend, const std::shared_ptr<IFilter> &filter,
                std::shared_ptr<IImuPayloadDecoder> decoder);
    ImuStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend, std::vector<std::shared_ptr<IFilter>> filters,
                std::shared_ptr<IImuPayloadDecoder> decoder);
    virtual ~ImuStreamer() noexcept override;

    virtual void startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override;
    virtual void stopStream(std::shared_ptr<const StreamProfile> profile) override;

    IDevice *getOwner() const override;

private:
    void         initFilters();
    virtual void parseIMUData(std::shared_ptr<Frame> frame);
    virtual void outputFrame(std::shared_ptr<Frame> frame);

private:
    IDevice                              *owner_;
    std::shared_ptr<IDataStreamPort>      backend_;
    std::vector<std::shared_ptr<IFilter>> filters_;
    std::shared_ptr<IImuPayloadDecoder>   decoder_;

    std::mutex                                                           cbMtx_;
    std::map<std::shared_ptr<const StreamProfile>, MutableFrameCallback> callbacks_;

    std::atomic_bool running_;
    uint64_t         frameIndex_;
    int32_t          ignoreLeadingFrameCount_ = 0;
};
}  // namespace libobsensor
