// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EgoProNetDevice.hpp"
#include "EGOAlgParamManager.hpp"
#include "EGOStreamStrategy.hpp"
#include "environment/EnvConfig.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "sensor/imu/ImuStreamer.hpp"
#include "sensor/imu/AccelSensor.hpp"
#include "sensor/imu/GyroSensor.hpp"
#include "sensor/audio/AudioSensor.hpp"
#include "hid/EgoStateService.hpp"
#include "hid/BatteryInfoService.hpp"
#include "hid/DirectionKeyService.hpp"
#include "hid/SharedDataStreamPort.hpp"

#include "metadata/FrameMetadataParserContainer.hpp"
#include "timestamp/FrameTimestampCalculator.hpp"
#include "timestamp/DeviceClockSynchronizer.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "property/PropertyServer.hpp"
#include "property/CommonPropertyAccessors.hpp"
#include "property/FilterPropertyAccessors.hpp"
#include "component/fwlog/FirmwareLogManager.hpp"
#include "component/fwrecord/FwRecordProfileManager.hpp"
#include "component/fwrecord/FwRecordingDirectoryManager.hpp"
#include "component/fwrecord/RecordingSessionResultManager.hpp"
#include "component/custommetadata/CustomMetadataManager.hpp"
#include "component/timezone/HostTimeInfoManager.hpp"
#include "component/httpfileservice/HttpFileServiceManager.hpp"
#include "common/DeviceSeriesInfo.hpp"

#include "FilterFactory.hpp"
#include "publicfilters/IMUCorrector.hpp"

#include "utils/BufferParser.hpp"
#include "utils/PublicTypeHelper.hpp"
#include "utils/Utils.hpp"

#include "firmwareupdater/FirmwareUpdater.hpp"
#include "monitor/EgoDeviceMonitor.hpp"
#include "monitor/DeviceActivityRecorder.hpp"

#if defined(BUILD_NET_PAL)
#include "ethernet/RTSPStreamPort.hpp"
#include "ethernet/VendorNetDataPort.hpp"
#endif

#include <algorithm>

namespace libobsensor {

namespace {
#if defined(BUILD_NET_PAL)
constexpr uint32_t egoVendorConnectTimeoutMs = 2000;
constexpr uint32_t egoVendorCommTimeoutMs    = 5000;
#endif

OBFormat normalizeH265AliasFormat(OBFormat format) {
    return format == OB_FORMAT_HEVC ? OB_FORMAT_H265 : format;
}

std::shared_ptr<const StreamProfile> normalizeH265AliasProfile(const std::shared_ptr<const StreamProfile> &profile) {
    if(profile && profile->getFormat() == OB_FORMAT_HEVC) {
        return profile->clone(OB_FORMAT_H265);
    }
    return profile;
}
}  // namespace

EgoProNetDevice::EgoProNetDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
    init();
    LOG_DEBUG("EGOProNetDevice Construct.....");
}

EgoProNetDevice::~EgoProNetDevice() noexcept {
    LOG_DEBUG("EGOProNetDevice Destructor.....");
}

void EgoProNetDevice::init() {
    initProperties();

    fetchDeviceInfo();
    fetchExtensionInfo();

    if(!inRecoveryMode_) {
        initSensorList();

        fetchAllVideoStreamProfileList();

        deviceTimeFreq_ = 1000000;
        // auto algParamManager = std::make_shared<EGOAlgParamManager>(this);
        // registerComponent(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, algParamManager);

        auto deviceClockSynchronizer = std::make_shared<DeviceClockSynchronizer>(this, deviceTimeFreq_, deviceTimeFreq_);
        registerComponent(OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER, deviceClockSynchronizer);
        registerComponent(OB_DEV_COMPONENT_FW_RECORD_PROFILE_MANAGER, [this]() { return std::make_shared<FwRecordProfileManager>(this); });

        // // SD-card HTTP file service (port 8080) — requires firmware >= 1.0.9
        // if(getFirmwareVersionInt() >= 10108) {
            auto netPortInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(enumInfo_->getSourcePortInfoList().front());
            if(netPortInfo) {
                const auto deviceIp = netPortInfo->address;
                registerComponent(OB_DEV_COMPONENT_HTTP_FILE_SERVICE_MANAGER, [this, deviceIp]() {
                    return std::make_shared<HttpFileServiceManager>(this, deviceIp);
                });
            }
            else {
                LOG_WARN("EGONetDevice: HTTP file service unavailable — network port info is missing");
            }
        // }
        // else {
        //     LOG_DEBUG("EGONetDevice: HTTP file service not supported by this firmware (requires >= 1.0.9, got {})",
        //               deviceInfo_->fwVersion_);
        // }
    }

    // firmware updater
    registerComponent(OB_DEV_COMPONENT_FIRMWARE_UPDATER, [this]() {
        std::shared_ptr<FirmwareUpdater> firmwareUpdater;
        TRY_EXECUTE({ firmwareUpdater = std::make_shared<FirmwareUpdater>(this); })
        return firmwareUpdater;
    });

    registerComponent(
        OB_DEV_COMPONENT_DEVICE_ACTIVITY_RECORDER,
        [this]() {
            std::shared_ptr<DeviceActivityRecorder> activityRecorder;
            TRY_EXECUTE({ activityRecorder = std::make_shared<DeviceActivityRecorder>(this); })
            return activityRecorder;
        },
        false);

    auto hostTimeInfoManager = std::make_shared<HostTimeInfoManager>(this);
    registerComponent(OB_DEV_COMPONENT_HOST_TIME_INFO_MANAGER, hostTimeInfoManager);
    BEGIN_TRY_EXECUTE({ hostTimeInfoManager->setHostTimeInfoToDevice(); })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_WARN("Failed to initialize host time information for EGO Pro network device"); })

    if(getFirmwareVersionInt() >= 10004) {
        auto propertyServer         = getPropertyServer();
        auto vendorPropertyAccessor = getComponentT<VendorPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        propertyServer->registerProperty(OB_STRUCT_DEVICE_MODEL_TYPE, "r", "r", vendorPropertyAccessor.get());
        propertyServer->registerProperty(OB_STRUCT_EGO_STATE_REPORT, "r", "r", vendorPropertyAccessor.get());
        propertyServer->registerProperty(OB_STRUCT_RECORDING_SESSION_RESULT, "", "rw", vendorPropertyAccessor.get());
        registerComponent(OB_DEV_COMPONENT_RECORDING_SESSION_RESULT_MANAGER,
                          std::make_shared<RecordingSessionResultManager>(this, vendorPropertyAccessor.get()));
    }
}

void EgoProNetDevice::fetchDeviceInfo() {
    auto portInfo           = enumInfo_->getSourcePortInfoList().front();
    auto netPortInfo        = std::dynamic_pointer_cast<const NetSourcePortInfo>(portInfo);
    auto deviceInfo         = std::make_shared<NetDeviceInfo>();
    deviceInfo->ipAddress_  = netPortInfo->address;
    deviceInfo->subnetMask_ = netPortInfo->mask;
    deviceInfo->gateway_    = netPortInfo->gateway;
    deviceInfo_             = deviceInfo;

    deviceInfo_->name_           = enumInfo_->getName();
    deviceInfo_->pid_            = enumInfo_->getPid();
    deviceInfo_->vid_            = enumInfo_->getVid();
    deviceInfo_->uid_            = enumInfo_->getUid();
    deviceInfo_->connectionType_ = enumInfo_->getConnectionType();

    auto propServer                   = getPropertyServer();
    auto version                      = propServer->getStructureDataT<OBVersionInfo>(OB_STRUCT_VERSION);
    deviceInfo_->name_                = version.deviceName;
    deviceInfo_->fwVersion_           = version.firmwareVersion;
    deviceInfo_->deviceSn_            = version.serialNumber;
    deviceInfo_->asicName_            = version.depthChip;
    deviceInfo_->hwVersion_           = version.hardwareVersion;
    deviceInfo_->type_                = static_cast<uint16_t>(version.deviceType);
    deviceInfo_->supportedSdkVersion_ = version.sdkVersion;
    deviceInfo_->subSystemVersion_    = version.subSystemVersion;

    std::string manufacturerName;
    auto        vid = deviceInfo_->vid_;
    auto        pid = deviceInfo_->pid_;
    auto        it  = std::find_if(EGODeviceInfoList.begin(), EGODeviceInfoList.end(),
                                   [vid, pid](const DeviceInfoEntry &entry) { return entry.vid_ == vid && entry.pid_ == pid; });

    if(it != EGODeviceInfoList.end()) {
        manufacturerName = std::string(it->manufacturer_);
    }

    if(deviceInfo_->name_.find("Orbbec ") == 0) {
        deviceInfo_->name_ = deviceInfo_->name_.substr(7);
    }
    deviceInfo_->fullName_ = deviceInfo_->name_;

    const size_t         deviceUserNameBufferSize = 32;
    std::vector<uint8_t> userNameData;
    BEGIN_TRY_EXECUTE({ userNameData = propServer->getStructureData(OB_STRUCT_DEVICE_USER_NAME, PROP_ACCESS_INTERNAL); })
    CATCH_EXCEPTION_AND_EXECUTE({ userNameData.clear(); })

    if(userNameData.size() == deviceUserNameBufferSize) {
        const auto terminator = std::find(userNameData.begin(), userNameData.end(), static_cast<uint8_t>(0));
        if(terminator != userNameData.begin() && terminator != userNameData.end()) {
            const std::string userName(userNameData.begin(), terminator);
            deviceInfo_->name_     = userName;
            deviceInfo_->fullName_ = userName;
        }
    }

    // mark the device as a multi-sensor device with same clock at default
    extensionInfo_["AllSensorsUsingSameClock"] = "true";
}

void EgoProNetDevice::initSensorList() {
    registerComponent(OB_DEV_COMPONENT_SENSOR_STREAM_STRATEGY, [this]() { return std::make_shared<EGOStreamStrategy>(this); });

    registerComponent(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY, [this]() {
        std::shared_ptr<FrameProcessorFactory> factory;
        TRY_EXECUTE({ factory = std::make_shared<FrameProcessorFactory>(this); })
        return factory;
    });

    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();

    // ---- Left color (RTSP) ----
    auto leftColorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_RTSP && std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo)->streamType == OB_STREAM_COLOR_LEFT;
    });

    if(leftColorPortInfoIter != sourcePortInfoList.end()) {
        auto leftColorPortInfo = *leftColorPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_LEFT_COLOR_SENSOR,
            [this, leftColorPortInfo]() {
                auto port   = getSourcePort(leftColorPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_COLOR_LEFT, port);
                sensor->enableTimestampAnomalyDetection(false);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGR, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGRA, OB_FORMAT_ANY, nullptr },
                };

                sensor->updateFormatFilterConfig(formatFilterConfigs);

                auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorBaseDeviceTime>(this, deviceTimeFreq_, colorFrameTimeFreq_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_LEFT_COLOR_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_COLOR_LEFT, leftColorPortInfo);
        registerComponent(OB_DEV_COMPONENT_LEFT_COLOR_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_COLOR_LEFT);
            return frameProcessor;
        });
    }

    // ---- Right color (RTSP) ----
    auto rightColorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_RTSP && std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo)->streamType == OB_STREAM_COLOR_RIGHT;
    });

    if(rightColorPortInfoIter != sourcePortInfoList.end()) {
        auto rightColorPortInfo = *rightColorPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_RIGHT_COLOR_SENSOR,
            [this, rightColorPortInfo]() {
                auto port   = getSourcePort(rightColorPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_COLOR_RIGHT, port);
                sensor->enableTimestampAnomalyDetection(false);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGR, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGRA, OB_FORMAT_ANY, nullptr },
                };

                sensor->updateFormatFilterConfig(formatFilterConfigs);

                auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorBaseDeviceTime>(this, deviceTimeFreq_, colorFrameTimeFreq_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_RIGHT_COLOR_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_COLOR_RIGHT, rightColorPortInfo);
        registerComponent(OB_DEV_COMPONENT_RIGHT_COLOR_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_COLOR_RIGHT);
            return frameProcessor;
        });
    }

    // ---- Side left color (RTSP) ----
    auto sideLeftColorPortInfoIter =
        std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
            return portInfo->portType == SOURCE_PORT_NET_RTSP
                   && std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo)->streamType == OB_STREAM_COLOR_SIDE_LEFT;
        });

    if(sideLeftColorPortInfoIter != sourcePortInfoList.end()) {
        auto sideLeftColorPortInfo = *sideLeftColorPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_SIDE_LEFT_COLOR_SENSOR,
            [this, sideLeftColorPortInfo]() {
                auto port   = getSourcePort(sideLeftColorPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_COLOR_SIDE_LEFT, port);
                sensor->enableTimestampAnomalyDetection(false);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGR, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGRA, OB_FORMAT_ANY, nullptr },
                };

                sensor->updateFormatFilterConfig(formatFilterConfigs);

                auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorBaseDeviceTime>(this, deviceTimeFreq_, colorFrameTimeFreq_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_SIDE_LEFT_COLOR_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_COLOR_SIDE_LEFT, sideLeftColorPortInfo);
        registerComponent(OB_DEV_COMPONENT_SIDE_LEFT_COLOR_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_COLOR_SIDE_LEFT);
            return frameProcessor;
        });
    }

    // ---- Side right color (RTSP) ----
    auto sideRightColorPortInfoIter =
        std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
            return portInfo->portType == SOURCE_PORT_NET_RTSP
                   && std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo)->streamType == OB_STREAM_COLOR_SIDE_RIGHT;
        });

    if(sideRightColorPortInfoIter != sourcePortInfoList.end()) {
        auto sideRightColorPortInfo = *sideRightColorPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_SIDE_RIGHT_COLOR_SENSOR,
            [this, sideRightColorPortInfo]() {
                auto port   = getSourcePort(sideRightColorPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_COLOR_SIDE_RIGHT, port);
                sensor->enableTimestampAnomalyDetection(false);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGR, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGRA, OB_FORMAT_ANY, nullptr },
                };

                sensor->updateFormatFilterConfig(formatFilterConfigs);

                auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorBaseDeviceTime>(this, deviceTimeFreq_, colorFrameTimeFreq_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_SIDE_RIGHT_COLOR_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_COLOR_SIDE_RIGHT, sideRightColorPortInfo);
        registerComponent(OB_DEV_COMPONENT_SIDE_RIGHT_COLOR_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_COLOR_SIDE_RIGHT);
            return frameProcessor;
        });
    }

    // ---- Audio (independent RTSP stream, vendor codec OB_FMT_PCM) ----
    auto audioPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_RTSP && std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo)->streamType == OB_STREAM_AUDIO;
    });

    if(audioPortInfoIter != sourcePortInfoList.end()) {
        auto audioPortInfo = *audioPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_AUDIO_SENSOR,
            [this, audioPortInfo]() {
                auto port   = getSourcePort(audioPortInfo);
                auto sensor = std::make_shared<AudioSensor>(this, port);

                // RTSP backend does not expose audio profiles; EgoPro network audio is PCM16 stereo,
                // 48 kHz, delivered in 2048-sample chunks.
                StreamProfileList audioProfiles;
                audioProfiles.push_back(StreamProfileFactory::createAudioStreamProfile(OB_FORMAT_PCM, 48000, 2, 16, 0, 2048));
                sensor->setStreamProfileList(audioProfiles);

                // WiFi audio carries OBNetworkFrameHeader.timestamp (device clock, same source as video/IMU).
                // Use the device time and color frame time frequency to calculate frame timestamps.
                auto frameTimestampCalculator = std::make_shared<FrameTimestampCalculatorBaseDeviceTime>(this, deviceTimeFreq_, colorFrameTimeFreq_);
                sensor->setFrameTimestampCalculator(frameTimestampCalculator);

                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_AUDIO, audioPortInfo);
    }

    // ---- IMU (private TCP data stream)
    auto imuPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_VENDOR_STREAM;
    });

    if(imuPortInfoIter != sourcePortInfoList.end()) {
        auto imuPortInfo = *imuPortInfoIter;
        registerComponent(OB_DEV_COMPONENT_SHARED_HID_STREAM, [this, imuPortInfo]() {
            auto port           = getSourcePort(imuPortInfo);
            auto dataStreamPort = std::dynamic_pointer_cast<IDataStreamPort>(port);
            auto sharedHidPort  = std::make_shared<SharedDataStreamPort>(this, dataStreamPort);
            return sharedHidPort;
        });

        registerComponent(OB_DEV_COMPONENT_IMU_STREAMER, [this, imuPortInfo]() {
            // no correction filter applied: EGOAlgParamManager is disabled, so no intrinsics would be bound
            auto sharedHidPort  = getComponentT<SharedDataStreamPort>(OB_DEV_COMPONENT_SHARED_HID_STREAM);
            auto dataStreamPort = sharedHidPort->createClientPort();

            std::shared_ptr<IImuPayloadDecoder> decoder = std::make_shared<LegacyImuCalculatorDecoder>(std::make_shared<ImuCalculatorBMI088>());

            auto imuStreamer = std::make_shared<ImuStreamer>(this, dataStreamPort, std::vector<std::shared_ptr<IFilter>>{}, decoder);
            return imuStreamer;
        });

        registerComponent(OB_DEV_COMPONENT_EGO_STATE_SERVICE, [this]() {
            auto sharedHidPort = getComponentT<SharedDataStreamPort>(OB_DEV_COMPONENT_SHARED_HID_STREAM);
            return std::make_shared<EgoStateService>(this, sharedHidPort->createClientPort());
        });

        registerComponent(OB_DEV_COMPONENT_BATTERY_INFO_SERVICE, [this]() {
            auto sharedHidPort = getComponentT<SharedDataStreamPort>(OB_DEV_COMPONENT_SHARED_HID_STREAM);
            return std::make_shared<BatteryInfoService>(this, sharedHidPort->createClientPort());
        });

        registerComponent(OB_DEV_COMPONENT_DIRECTION_KEY_SERVICE, [this]() {
            auto sharedHidPort = getComponentT<SharedDataStreamPort>(OB_DEV_COMPONENT_SHARED_HID_STREAM);
            return std::make_shared<DirectionKeyService>(this, sharedHidPort->createClientPort());
        });

        registerComponent(
            OB_DEV_COMPONENT_ACCEL_SENSOR,
            [this, imuPortInfo]() {
                auto port                 = getSourcePort(imuPortInfo);
                auto imuStreamer          = getComponentT<ImuStreamer>(OB_DEV_COMPONENT_IMU_STREAMER);
                auto imuStreamerSharedPtr = imuStreamer.get();
                auto sensor               = std::make_shared<AccelSensor>(this, port, imuStreamerSharedPtr, true);

                // [EGO-WIP] EGO firmware does not yet provide a reliable per-frame IMU device timestamp, which
                // trips the anomaly detector and drops every frame. Disable it until the timestamp is fixed.
                sensor->enableTimestampAnomalyDetection(false);

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_ACCEL, imuPortInfo);

        registerComponent(
            OB_DEV_COMPONENT_GYRO_SENSOR,
            [this, imuPortInfo]() {
                auto port                 = getSourcePort(imuPortInfo);
                auto imuStreamer          = getComponentT<ImuStreamer>(OB_DEV_COMPONENT_IMU_STREAMER);
                auto imuStreamerSharedPtr = imuStreamer.get();
                auto sensor               = std::make_shared<GyroSensor>(this, port, imuStreamerSharedPtr, true);

                // [EGO-WIP] EGO firmware does not yet provide a reliable per-frame IMU device timestamp, which
                // trips the anomaly detector and drops every frame. Disable it until the timestamp is fixed.
                sensor->enableTimestampAnomalyDetection(false);

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_GYRO, imuPortInfo);
    }
}

void EgoProNetDevice::initProperties() {
    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();
    auto        vendorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(),
                                                  [](const std::shared_ptr<const SourcePortInfo> &portInfo) { return portInfo->portType == SOURCE_PORT_NET_VENDOR; });

    if(vendorPortInfoIter == sourcePortInfoList.end()) {
        return;
    }

    auto propertyServer = std::make_shared<PropertyServer>(this);

    auto vendorPortInfo = *vendorPortInfoIter;
    auto sourcePort     = getSourcePort(vendorPortInfo);
#if defined(BUILD_NET_PAL)
    auto vendorNetPort  = std::dynamic_pointer_cast<VendorNetDataPort>(sourcePort);
    if(!vendorNetPort) {
        THROW_INVALID_PARAM_EXCEPTION("EGO Pro network vendor port is not VendorNetDataPort");
    }
    vendorNetPort->setTcpTimeouts(egoVendorConnectTimeoutMs, egoVendorCommTimeoutMs);
#endif

    auto vendorPropertyAccessor = std::make_shared<LazySuperPropertyAccessor>([this, sourcePort]() {
        return std::make_shared<VendorPropertyAccessor>(this, sourcePort);
    });

    // the main property accessor
    registerComponent(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR, [this, sourcePort]() {
        return std::make_shared<VendorPropertyAccessor>(this, sourcePort);
    });

    registerComponent(OB_DEV_COMPONENT_DEVICE_MONITOR, [this, sourcePort]() {
        return std::make_shared<EgoDeviceMonitor>(this, sourcePort);
    });

    // color controls (apply to both left/right RGB)
    propertyServer->registerProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_GAIN_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_WHITE_BALANCE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_BRIGHTNESS_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_SATURATION_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_SHARPNESS_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_CONTRAST_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, "rw", "rw", vendorPropertyAccessor);

    // device / IMU controls
    propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_HOST_TIME_INFO, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_SERIAL_NUMBER, "r", "r", vendorPropertyAccessor);
    // propertyServer->registerProperty(OB_STRUCT_DEVICE_IP_ADDR_CONFIG, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_TIME, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_GET_TF_CARD_CAPACITY, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_TEMPERATURE, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_REBOOT_DEVICE_BOOL, "w", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_RECORDING_PROMPT_INT, "rw", "w", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_PROP_GYRO_ODR_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_ACCEL_ODR_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_GYRO_FULL_SCALE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_ACCEL_FULL_SCALE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_GET_ACCEL_PRESETS_ODR_LIST, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_GET_ACCEL_PRESETS_FULL_SCALE_LIST, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_GET_GYRO_PRESETS_ODR_LIST, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_GET_GYRO_PRESETS_FULL_SCALE_LIST, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_ACCEL_SWITCH_BOOL, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_GYRO_SWITCH_BOOL, "", "rw", vendorPropertyAccessor);
    // propertyServer->registerProperty(OB_RAW_DATA_IMU_CALIB_PARAM, "", "rw", vendorPropertyAccessor);
    // propertyServer->registerProperty(OB_RAW_DATA_ALIGN_CALIB_PARAM, "", "r", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_RAW_DATA_ALIGN_CALIB_YAML, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_IMU_CALIB_YAML, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_EXPORT_LOG, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_BITRATE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_DYNAMIC_BITRATE_ENABLE_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_AE_MAX_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_RAW_DATA_STREAM_PROFILE_LIST, "r", "r", vendorPropertyAccessor);
    // EgoPro firmware does not support OB_PROP_FW_RECORDING_STATE_BOOL yet.
    propertyServer->registerProperty(OB_PROP_FW_RECORDING_STATE_BOOL, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_FW_RECORDING_FILE_SPLIT_DURATION_MIN_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_RECORDING_DESTINATION_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_FW_RECORDING_FORMAT_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_VIDEO_GOP_INT, "rw", "rw", vendorPropertyAccessor);
    auto recordingDirectoryAccessor = std::make_shared<FwRecordingDirectoryManager>(this, vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_FW_RECORDING_DIRECTORY, "rw", "rw", recordingDirectoryAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_USER_NAME, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_BLUETOOTH_PAIRING_INFO, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_BLUETOOTH_CLEAR_PAIRING_INT, "w", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_CLEAR_WIFI_CONFIG_INT, "w", "w", vendorPropertyAccessor);
    auto customMetadataManager = std::make_shared<CustomMetadataManager>(this, vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_CUSTOM_METADATA_JSON, "w", "w", customMetadataManager);
    registerComponent(OB_DEV_COMPONENT_CUSTOM_METADATA_MANAGER, customMetadataManager);
    propertyServer->registerProperty(OB_PROP_DEVICE_VOICE_PROMPT_LANGUAGE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_WRIST_WEAR_SIDE_INT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_FW_RECORDING_PROFILE_LIST, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_FW_RECORDING_MCAP_TOPIC_LIST, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_FW_RECORDING_PART_LIST, "r", "r", vendorPropertyAccessor);

    // propertyServer->registerProperty(OB_PROP_DEVICE_IN_RECOVERY_MODE_BOOL, "r", "r", vendorPropertyAccessor);
    // propertyServer->registerProperty(OB_PROP_BOOT_INTO_RECOVERY_MODE_BOOL, "w", "w", vendorPropertyAccessor);

    auto imuCorrectorFilter = getSensorFrameFilter("IMUCorrector", OB_SENSOR_ACCEL);
    if(imuCorrectorFilter) {
        auto filterStateProperty = std::make_shared<FilterStatePropertyAccessor>(imuCorrectorFilter);
        propertyServer->registerProperty(OB_PROP_SDK_ACCEL_FRAME_TRANSFORMED_BOOL, "rw", "rw", filterStateProperty);
        propertyServer->registerProperty(OB_PROP_SDK_GYRO_FRAME_TRANSFORMED_BOOL, "rw", "rw", filterStateProperty);
    }

    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, false);
    registerComponent(OB_DEV_COMPONENT_FW_LOG_MANAGER, [this]() { return std::make_shared<FirmwareLogManager>(this); });

    // [EGO-WIP] The following commands send to port 8090 and crash the firmware TCP service.
    // Re-enable once firmware implements these properties.
    // BEGIN_TRY_EXECUTE({ propertyServer->setPropertyValueT(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, OB_COMM_NET); })
    // CATCH_EXCEPTION_AND_EXECUTE({ LOG_ERROR("Set device communication type to ethernet mode failed!"); })

    // check recovery mode
    // inRecoveryMode_ = false;
    // BEGIN_TRY_EXECUTE({
    //     inRecoveryMode_ = propertyServer->getPropertyValueT<bool>(OB_PROP_DEVICE_IN_RECOVERY_MODE_BOOL);
    //     if(inRecoveryMode_) {
    //         LOG_DEBUG("EGO net device is in recovery mode, skip init ...");
    //         propertyServer->unregisterAllProperties();
    //         propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor);
    //         propertyServer->registerProperty(OB_PROP_BOOT_INTO_RECOVERY_MODE_BOOL, "w", "w", vendorPropertyAccessor);
    //         propertyServer->registerProperty(OB_PROP_DEVICE_IN_RECOVERY_MODE_BOOL, "r", "r", vendorPropertyAccessor);
    //         return;
    //     }
    // })
    // CATCH_EXCEPTION_AND_EXECUTE({ LOG_DEBUG("Get device in recovery mode failed!"); })
}

void EgoProNetDevice::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {
    auto              sensorType = sensor->getSensorType();
    OBStreamType      streamType = utils::mapSensorTypeToStreamType(sensorType);
    StreamProfileList profileList;
    for(const auto &profile: allVideoStreamProfileList_) {
        if(streamType == profile->getType()) {
            profileList.push_back(profile);
        }
    }

    if(profileList.empty() && (streamType == OB_STREAM_COLOR_LEFT || streamType == OB_STREAM_COLOR_RIGHT || streamType == OB_STREAM_COLOR_SIDE_LEFT || streamType == OB_STREAM_COLOR_SIDE_RIGHT)) {
        // Firmware has not yet implemented OB_RAW_DATA_STREAM_PROFILE_LIST; provide a minimal
        // hardcoded fallback so the RTSP stream can be opened for early bring-up testing.
        // profileList.push_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1600, 1300, 30));
        profileList.push_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_H264, 1600, 1200, 30));
        profileList.push_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_H264, 1600, 1200, 60));
        profileList.push_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_H265, 1600, 1200, 30));
        profileList.push_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_H265, 1600, 1200, 60));
        LOG_WARN("EGONetDevice: using hardcoded fallback stream profiles for sensor {}", sensorType);
    }

    if(!profileList.empty()) {
        sensor->setStreamProfileList(profileList);
    }

    auto streamProfile = StreamProfileFactory::getDefaultStreamProfileFromEnvConfig(deviceInfo_->name_, sensorType);
    if(streamProfile) {
        sensor->updateDefaultStreamProfile(normalizeH265AliasProfile(streamProfile));
    }

    auto profiles = sensor->getStreamProfileList();

    if(sensorType != OB_SENSOR_AUDIO) {
        auto algParamManager = getComponentT<EGOAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, false);
        if(algParamManager) {
            algParamManager->bindStreamProfileParams(profiles);
        }
    }

    LOG_DEBUG("Sensor {} created! Found {} stream profiles.", sensorType, profiles.size());
    for(auto &profile: profiles) {
        LOG_DEBUG(" - {}", profile);
    }
}

void EgoProNetDevice::fetchAllVideoStreamProfileList() {
    auto propServer = getPropertyServer();

    std::vector<uint8_t> data;
    BEGIN_TRY_EXECUTE({
        propServer->getRawData(
            OB_RAW_DATA_STREAM_PROFILE_LIST,
            [&](OBDataTranState state, OBDataChunk *dataChunk) {
                if(state == DATA_TRAN_STAT_TRANSFERRING) {
                    data.insert(data.end(), dataChunk->data, dataChunk->data + dataChunk->size);
                }
            },
            PROP_ACCESS_INTERNAL);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Get profile list params failed!");
        data.clear();
    })

    if(!data.empty()) {
        std::vector<OBInternalStreamProfile> outputProfiles;
        uint16_t                             dataSize = static_cast<uint16_t>(data.size());
        outputProfiles                                = parseBuffer<OBInternalStreamProfile>(data.data(), dataSize);
        allVideoStreamProfileList_.clear();
        for(const auto &item: outputProfiles) {
            OBStreamType streamType = utils::mapSensorTypeToStreamType((OBSensorType)item.sensorType);
            OBFormat     format     = normalizeH265AliasFormat(utils::uvcFourccToOBFormat(item.profile.video.formatFourcc));
            allVideoStreamProfileList_.push_back(StreamProfileFactory::createVideoStreamProfile(streamType, format, item.profile.video.width,
                                                                                                item.profile.video.height, item.profile.video.fps));
        }
    }
    else {
        LOG_WARN("Get stream profile list failed!");
    }
}

std::vector<std::shared_ptr<IFilter>> EgoProNetDevice::createRecommendedPostProcessingFilters(OBSensorType type) {
    utils::unusedVar(type);
    return {};
}

}  // namespace libobsensor
