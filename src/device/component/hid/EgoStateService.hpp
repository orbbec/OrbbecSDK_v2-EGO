// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceComponent.hpp"
#include "ISourcePort.hpp"
#include "libobsensor/h/ObTypes.h"

#include <functional>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace libobsensor {

class EgoStateService : public IDeviceComponent {
public:
    using EgoStateCallback = std::function<void(const OBEgoStateReport &)>;

    struct EgoRecordingResult {
        OBEgoStateReport         state{};
        std::string              recordingDirectory;
        std::vector<std::string> partNames;
        bool                     directoryValid = false;
        bool                     partListValid  = false;
    };
    using EgoRecordingResultCallback = std::function<void(const EgoRecordingResult &)>;

    EgoStateService(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend);
    ~EgoStateService() noexcept override;

    void     setCallback(EgoStateCallback callback);
    void     clearCallback();
    OBEgoStateReport getState() const;
    void     setRecordingResultCallback(EgoRecordingResultCallback callback);
    void     clearRecordingResultCallback();
    IDevice *getOwner() const override;

private:
    void onFrame(std::shared_ptr<Frame> frame);
    void notifyCurrentStateAfterCallbackRegistered();
    void startBackendIfNeeded();
    void stopBackendIfUnused();
    void enqueueRecordingResultTask(const OBEgoStateReport &state);
    void recordingResultWorkerLoop();

private:
    IDevice                         *owner_;
    std::shared_ptr<IDataStreamPort> backend_;

    std::mutex                    mutex_;
    EgoStateCallback              callback_;
    EgoRecordingResultCallback    recordingResultCallback_;
    std::unordered_set<uint32_t>  processedResultSequences_;
    bool                          running_ = false;
    std::mutex                    backendMutex_;

    std::mutex                      workerMutex_;
    std::condition_variable         workerCv_;
    std::deque<OBEgoStateReport>    pendingRecordingResultTasks_;
    bool                            workerExit_ = false;
    std::thread                     workerThread_;
};

}  // namespace libobsensor
