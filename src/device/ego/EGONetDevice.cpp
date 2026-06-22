// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EGONetDevice.hpp"
#include "EGOAlgParamManager.hpp"
#include "environment/EnvConfig.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "sensor/imu/ImuStreamer.hpp"
#include "sensor/imu/AccelSensor.hpp"
#include "sensor/imu/GyroSensor.hpp"
#include "sensor/audio/AudioSensor.hpp"

#include "metadata/FrameMetadataParserContainer.hpp"
#include "timestamp/GlobalTimestampFitter.hpp"
#include "timestamp/FrameTimestampCalculator.hpp"
#include "timestamp/DeviceClockSynchronizer.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "property/PropertyServer.hpp"
#include "property/CommonPropertyAccessors.hpp"
#include "property/FilterPropertyAccessors.hpp"

#include "FilterFactory.hpp"
#include "publicfilters/IMUCorrector.hpp"

#include "utils/BufferParser.hpp"
#include "utils/PublicTypeHelper.hpp"
#include "utils/Utils.hpp"

#include "firmwareupdater/FirmwareUpdater.hpp"
#include "monitor/DeviceMonitor.hpp"

#if defined(BUILD_NET_PAL)
#include "ethernet/RTSPStreamPort.hpp"
#endif

namespace libobsensor {

EGONetDevice::EGONetDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
    init();

    // check and start heartbeat after initialization is complete
    checkAndStartHeartbeat();
}

EGONetDevice::~EGONetDevice() noexcept {}

void EGONetDevice::init() {
    initProperties();

    fetchDeviceInfo();
    fetchExtensionInfo();

    if(!inRecoveryMode_) {
        initSensorList();
        fetchAllVideoStreamProfileList();

        deviceTimeFreq_ = 1000000;

        auto globalTimestampFilter = std::make_shared<GlobalTimestampFitter>(this);
        registerComponent(OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER, globalTimestampFilter);

        auto algParamManager = std::make_shared<EGOAlgParamManager>(this);
        registerComponent(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, algParamManager);

        auto deviceClockSynchronizer = std::make_shared<DeviceClockSynchronizer>(this, deviceTimeFreq_, deviceTimeFreq_);
        registerComponent(OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER, deviceClockSynchronizer);
    }

    // firmware updater
    registerComponent(OB_DEV_COMPONENT_FIRMWARE_UPDATER, [this]() {
        std::shared_ptr<FirmwareUpdater> firmwareUpdater;
        TRY_EXECUTE({ firmwareUpdater = std::make_shared<FirmwareUpdater>(this); })
        return firmwareUpdater;
    });
}

void EGONetDevice::fetchDeviceInfo() {
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
    deviceInfo_->fwVersion_           = version.firmwareVersion;
    deviceInfo_->deviceSn_            = version.serialNumber;
    deviceInfo_->asicName_            = version.depthChip;
    deviceInfo_->hwVersion_           = version.hardwareVersion;
    deviceInfo_->type_                = static_cast<uint16_t>(version.deviceType);
    deviceInfo_->supportedSdkVersion_ = version.sdkVersion;

    // remove the prefix "Orbbec " from the device name if contained
    if(deviceInfo_->name_.find("Orbbec ") == 0) {
        deviceInfo_->name_ = deviceInfo_->name_.substr(7);
    }
    deviceInfo_->fullName_ = "Orbbec " + deviceInfo_->name_;

    // mark the device as a multi-sensor device with same clock at default
    extensionInfo_["AllSensorsUsingSameClock"] = "true";
}

void EGONetDevice::initSensorList() {
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

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                };
                auto formatConverter = getSensorFrameFilter("FormatConverter", OB_SENSOR_COLOR_LEFT, false);
                if(formatConverter) {
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_MJPG, OB_FORMAT_RGB, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_MJPG, OB_FORMAT_BGR, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_MJPG, OB_FORMAT_BGRA, formatConverter });
                }
                sensor->updateFormatFilterConfig(formatFilterConfigs);

                auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorBaseDeviceTime>(this, deviceTimeFreq_, colorFrameTimeFreq_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

                // Left/right color share the single device GlobalTimestampFitter -> same device clock domain.
                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, colorFrameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

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

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                };
                auto formatConverter = getSensorFrameFilter("FormatConverter", OB_SENSOR_COLOR_RIGHT, false);
                if(formatConverter) {
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_MJPG, OB_FORMAT_RGB, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_MJPG, OB_FORMAT_BGR, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_MJPG, OB_FORMAT_BGRA, formatConverter });
                }
                sensor->updateFormatFilterConfig(formatFilterConfigs);

                auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorBaseDeviceTime>(this, deviceTimeFreq_, colorFrameTimeFreq_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, colorFrameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

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

                // RTSP backend does not expose audio profiles; provide a default PCM16 profile (16 kHz mono, 20 ms chunk).
                // Fields should be confirmed against firmware capabilities.
                StreamProfileList audioProfiles;
                audioProfiles.push_back(StreamProfileFactory::createAudioStreamProfile(OB_FORMAT_PCM, 16000, 1, 16, 0, 320));
                sensor->setStreamProfileList(audioProfiles);

                // WiFi audio carries OBNetworkFrameHeader.timestamp (device clock, same source as video/IMU).
                auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorBaseDeviceTime>(this, deviceTimeFreq_, deviceTimeFreq_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, deviceTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_AUDIO, audioPortInfo);
    }

    // ---- IMU (private TCP data stream, modeled on FemtoMega) ----
    auto imuPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_VENDOR_STREAM;
    });

    if(imuPortInfoIter != sourcePortInfoList.end()) {
        auto imuPortInfo = *imuPortInfoIter;
        registerComponent(OB_DEV_COMPONENT_IMU_STREAMER, [this, imuPortInfo]() {
            auto                                  port               = getSourcePort(imuPortInfo);
            auto                                  imuReversionFilter = getSensorFrameFilter("IMUFrameReversion", OB_SENSOR_ACCEL, true);
            auto                                  imuCorrectorFilter = getSensorFrameFilter("IMUCorrector", OB_SENSOR_ACCEL, true);
            std::vector<std::shared_ptr<IFilter>> imuFilters         = { imuReversionFilter, imuCorrectorFilter };
            auto                                  dataStreamPort     = std::dynamic_pointer_cast<IDataStreamPort>(port);
            auto                                  imuStreamer        = std::make_shared<ImuStreamer>(this, dataStreamPort, imuFilters);
            return imuStreamer;
        });

        registerComponent(
            OB_DEV_COMPONENT_ACCEL_SENSOR,
            [this, imuPortInfo]() {
                auto port                 = getSourcePort(imuPortInfo);
                auto imuStreamer          = getComponentT<ImuStreamer>(OB_DEV_COMPONENT_IMU_STREAMER);
                auto imuStreamerSharedPtr = imuStreamer.get();
                auto sensor               = std::make_shared<AccelSensor>(this, port, imuStreamerSharedPtr);

                // [EGO-WIP] EGO firmware does not yet provide a reliable per-frame IMU device timestamp, which
                // trips the anomaly detector and drops every frame. Disable it until the timestamp is fixed.
                sensor->enableTimestampAnomalyDetection(false);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, deviceTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

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
                auto sensor               = std::make_shared<GyroSensor>(this, port, imuStreamerSharedPtr);

                // [EGO-WIP] EGO firmware does not yet provide a reliable per-frame IMU device timestamp, which
                // trips the anomaly detector and drops every frame. Disable it until the timestamp is fixed.
                sensor->enableTimestampAnomalyDetection(false);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, deviceTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_GYRO, imuPortInfo);
    }
}

void EGONetDevice::initProperties() {
    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();
    auto        vendorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(),
                                                  [](const std::shared_ptr<const SourcePortInfo> &portInfo) { return portInfo->portType == SOURCE_PORT_NET_VENDOR; });

    if(vendorPortInfoIter == sourcePortInfoList.end()) {
        return;
    }

    auto propertyServer = std::make_shared<PropertyServer>(this);

    auto vendorPortInfo         = *vendorPortInfoIter;
    auto vendorPropertyAccessor = std::make_shared<LazySuperPropertyAccessor>([this, vendorPortInfo]() {
        auto port     = getSourcePort(vendorPortInfo);
        auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
        return accessor;
    });

    // the main property accessor
    registerComponent(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR, [this, vendorPortInfo]() {
        auto port     = getSourcePort(vendorPortInfo);
        auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
        return accessor;
    });

    registerComponent(OB_DEV_COMPONENT_DEVICE_MONITOR, [this, vendorPortInfo]() {
        auto port       = getSourcePort(vendorPortInfo);
        auto devMonitor = std::make_shared<DeviceMonitor>(this, port);
        return devMonitor;
    });

    // color controls (apply to both left/right RGB)
    propertyServer->registerProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_GAIN_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_WHITE_BALANCE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_BRIGHTNESS_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, "rw", "rw", vendorPropertyAccessor);

    // device / IMU controls
    propertyServer->registerProperty(OB_PROP_TIMESTAMP_OFFSET_INT, "rw", "rw", vendorPropertyAccessor);
    auto heartbeatPropertyAccessor = std::make_shared<HeartbeatPropertyAccessor>(this);
    propertyServer->registerProperty(OB_PROP_HEARTBEAT_BOOL, "rw", "rw", heartbeatPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_SERIAL_NUMBER, "r", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_IP_ADDR_CONFIG, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_TIME, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_REBOOT_DEVICE_BOOL, "w", "w", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_PROP_GYRO_ODR_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_ACCEL_ODR_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_GYRO_FULL_SCALE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_ACCEL_FULL_SCALE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_ACCEL_SWITCH_BOOL, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_GYRO_SWITCH_BOOL, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_IMU_CALIB_PARAM, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_ALIGN_CALIB_PARAM, "", "r", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_RAW_DATA_STREAM_PROFILE_LIST, "r", "r", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_PROP_DEVICE_IN_RECOVERY_MODE_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_BOOT_INTO_RECOVERY_MODE_BOOL, "w", "w", vendorPropertyAccessor);

    auto imuCorrectorFilter = getSensorFrameFilter("IMUCorrector", OB_SENSOR_ACCEL);
    if(imuCorrectorFilter) {
        auto filterStateProperty = std::make_shared<FilterStatePropertyAccessor>(imuCorrectorFilter);
        propertyServer->registerProperty(OB_PROP_SDK_ACCEL_FRAME_TRANSFORMED_BOOL, "rw", "rw", filterStateProperty);
        propertyServer->registerProperty(OB_PROP_SDK_GYRO_FRAME_TRANSFORMED_BOOL, "rw", "rw", filterStateProperty);
    }

    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, false);

    BEGIN_TRY_EXECUTE({ propertyServer->setPropertyValueT(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, OB_COMM_NET); })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_ERROR("Set device communication type to ethernet mode failed!"); })

    // check recovery mode
    inRecoveryMode_ = false;
    BEGIN_TRY_EXECUTE({
        inRecoveryMode_ = propertyServer->getPropertyValueT<bool>(OB_PROP_DEVICE_IN_RECOVERY_MODE_BOOL);
        if(inRecoveryMode_) {
            LOG_DEBUG("EGO net device is in recovery mode, skip init ...");
            propertyServer->unregisterAllProperties();
            propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_BOOT_INTO_RECOVERY_MODE_BOOL, "w", "w", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEVICE_IN_RECOVERY_MODE_BOOL, "r", "r", vendorPropertyAccessor);
            return;
        }
    })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_DEBUG("Get device in recovery mode failed!"); })
}

void EGONetDevice::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {
    auto              sensorType = sensor->getSensorType();
    OBStreamType      streamType = utils::mapSensorTypeToStreamType(sensorType);
    StreamProfileList profileList;
    for(const auto &profile: allVideoStreamProfileList_) {
        if(streamType == profile->getType()) {
            profileList.push_back(profile);
        }
    }

    if(!profileList.empty()) {
        sensor->setStreamProfileList(profileList);
    }

    auto streamProfile = StreamProfileFactory::getDefaultStreamProfileFromEnvConfig(deviceInfo_->name_, sensorType);
    if(streamProfile) {
        sensor->updateDefaultStreamProfile(streamProfile);
    }

    auto profiles = sensor->getStreamProfileList();

    // bind params: extrinsics, intrinsics, etc. (audio has no spatial calibration)
    if(sensorType != OB_SENSOR_AUDIO) {
        auto algParamManager = getComponentT<EGOAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER);
        algParamManager->bindStreamProfileParams(profiles);
    }

    LOG_DEBUG("Sensor {} created! Found {} stream profiles.", sensorType, profiles.size());
    for(auto &profile: profiles) {
        LOG_DEBUG(" - {}", profile);
    }
}

void EGONetDevice::fetchAllVideoStreamProfileList() {
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
            OBFormat     format     = utils::uvcFourccToOBFormat(item.profile.video.formatFourcc);
            allVideoStreamProfileList_.push_back(StreamProfileFactory::createVideoStreamProfile(streamType, format, item.profile.video.width,
                                                                                                item.profile.video.height, item.profile.video.fps));
        }
    }
    else {
        LOG_WARN("Get stream profile list failed!");
    }
}

std::vector<std::shared_ptr<IFilter>> EGONetDevice::createRecommendedPostProcessingFilters(OBSensorType type) {
    utils::unusedVar(type);
    return {};
}

}  // namespace libobsensor
