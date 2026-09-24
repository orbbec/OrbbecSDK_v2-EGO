// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EGODepthNetDevice.hpp"
#include "EGODepthDepthWorkModeManager.hpp"
#include "EGODepthPresetManager.hpp"
#include "EGODepthStreamProfileFilter.hpp"
#include "environment/EnvConfig.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "sensor/imu/ImuStreamer.hpp"
#include "sensor/imu/ImuPayloadDecoder.hpp"
#include "sensor/imu/AccelSensor.hpp"
#include "sensor/imu/GyroSensor.hpp"
#include "sensor/imu/MagSensor.hpp"
#include "sensor/audio/AudioSensor.hpp"
#include "hid/EgoStateService.hpp"
#include "hid/BatteryInfoService.hpp"
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
#include "component/custommetadata/CustomMetadataManager.hpp"
#include "component/timezone/HostTimeInfoManager.hpp"
#include "common/DeviceSeriesInfo.hpp"

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

#include <algorithm>
#include <cstring>

namespace libobsensor {

namespace {
OBFormat normalizeH265AliasFormat(OBFormat format) {
    return format == OB_FORMAT_HEVC ? OB_FORMAT_H265 : format;
}

std::shared_ptr<const StreamProfile> normalizeH265AliasProfile(const std::shared_ptr<const StreamProfile> &profile) {
    if(profile && profile->getFormat() == OB_FORMAT_HEVC) {
        return profile->clone(OB_FORMAT_H265);
    }
    return profile;
}

bool isColorStreamType(OBStreamType streamType) {
    return streamType == OB_STREAM_COLOR || streamType == OB_STREAM_COLOR_LEFT || streamType == OB_STREAM_COLOR_RIGHT || streamType == OB_STREAM_COLOR_TOP;
}

// Extracted so the BEGIN_TRY_EXECUTE macro doesn't have to wrap the template argument list in
// extra parentheses — the template's comma would otherwise be parsed as a second macro argument.
std::vector<OBEffectiveStreamProfile> readEffectiveVideoStreamProfileList(const DeviceComponentPtr<IPropertyServer> &propServer) {
    return propServer->getStructureDataListProtoV1_1_T<OBEffectiveStreamProfile, 0>(OB_RAW_DATA_EFFECTIVE_VIDEO_STREAM_PROFILE_LIST);
}
}  // namespace

EGODepthNetDevice::EGODepthNetDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
    init();

    // check and start heartbeat after initialization is complete
    checkAndStartHeartbeat();
    LOG_DEBUG("EGODepthNetDevice Construct.....");
}

EGODepthNetDevice::~EGODepthNetDevice() noexcept {
    LOG_DEBUG("EGODepthNetDevice Destructor.....");
}

void EGODepthNetDevice::init() {
    initProperties();

    // Depth work mode / preset support: same "Default" <-> "Factory Calib" semantics as the USB variant.
    // EGODepthDepthWorkModeManager is fault-tolerant: on firmware that does not implement the depth-alg-mode
    // structures its mode list stays empty and the preset list is reported as empty instead of throwing.
    // It must be registered before the preset manager, which resolves it on construction.
    auto depthWorkModeManager = std::make_shared<EGODepthDepthWorkModeManager>(this);
    registerComponent(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER, depthWorkModeManager);
    auto presetManager = std::make_shared<EGODepthPresetManager>(this);
    registerComponent(OB_DEV_COMPONENT_PRESET_MANAGER, presetManager);

    fetchDeviceInfo();
    fetchExtensionInfo();

    if(!inRecoveryMode_) {
        initSensorList();

        fetchAllVideoStreamProfileList();

        deviceTimeFreq_ = 1000000;

        auto deviceClockSynchronizer = std::make_shared<DeviceClockSynchronizer>(this, deviceTimeFreq_, deviceTimeFreq_);
        registerComponent(OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER, deviceClockSynchronizer);
        registerComponent(OB_DEV_COMPONENT_FW_RECORD_PROFILE_MANAGER, [this]() { return std::make_shared<FwRecordProfileManager>(this); });
    }

    // firmware updater
    registerComponent(OB_DEV_COMPONENT_FIRMWARE_UPDATER, [this]() {
        std::shared_ptr<FirmwareUpdater> firmwareUpdater;
        TRY_EXECUTE({ firmwareUpdater = std::make_shared<FirmwareUpdater>(this); })
        return firmwareUpdater;
    });

    auto hostTimeInfoManager = std::make_shared<HostTimeInfoManager>(this);
    registerComponent(OB_DEV_COMPONENT_HOST_TIME_INFO_MANAGER, hostTimeInfoManager);
    BEGIN_TRY_EXECUTE({ hostTimeInfoManager->setHostTimeInfoToDevice(); })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_WARN("Failed to initialize host time information for EGODepth network device"); })

    auto propertyServer         = getPropertyServer();
    auto vendorPropertyAccessor = getComponentT<VendorPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_MODEL_TYPE, "r", "r", vendorPropertyAccessor.get());

    // Trim the sensor/component tree for the active depth work mode. Structural changes only: init() runs
    // from the constructor, so nothing here may instantiate a sensor/component (see postInitialize()).
    fixSensorList();
}

void EGODepthNetDevice::postInitialize() {
    // Called by DeviceManager::createDevice() once construction is complete. Attaching the firmware's
    // effective stream profiles instantiates sensors, which would throw std::bad_weak_ptr from the
    // constructor (FrameProcessorFactory -> owner->shared_from_this()).
    // Mirrors EGODepthDevice::postInitialize() / G330NetDevice::postInitialize().
    BEGIN_TRY_EXECUTE({ applyEffectiveStreamProfileFilters(); })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_WARN("EGODepth(net): failed to apply effective stream profile filters"); })

    DeviceBase::postInitialize();
}

void EGODepthNetDevice::fetchDeviceInfo() {
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

void EGODepthNetDevice::registerNetVideoSensor(OBStreamType streamType, OBSensorType sensorType, DeviceComponentId sensorComponent,
                                               DeviceComponentId frameProcessorComponent) {
    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();

    auto portInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [streamType](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_RTSP && std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo)->streamType == streamType;
    });

    if(portInfoIter == sourcePortInfoList.end()) {
        return;
    }

    auto portInfo    = *portInfoIter;
    bool isColorType = isColorStreamType(streamType);

    registerComponent(
        sensorComponent,
        [this, portInfo, sensorType, frameProcessorComponent, isColorType]() {
            auto port   = getSourcePort(portInfo);
            auto sensor = std::make_shared<VideoSensor>(this, sensorType, port);
            sensor->enableTimestampAnomalyDetection(false);

            if(isColorType) {
                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGR, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGRA, OB_FORMAT_ANY, nullptr },
                };
                sensor->updateFormatFilterConfig(formatFilterConfigs);
            }

            // Compressed color streams carry 90 kHz RTP timestamps; raw Depth/IR carry the device clock.
            auto frameTimeFreq                 = isColorType ? colorFrameTimeFreq_ : deviceTimeFreq_;
            auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorBaseDeviceTime>(this, deviceTimeFreq_, frameTimeFreq);
            sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

            auto frameProcessor = getComponentT<FrameProcessor>(frameProcessorComponent, false);
            if(frameProcessor) {
                sensor->setFrameProcessor(frameProcessor.get());
            }

            initSensorStreamProfile(sensor);
            return sensor;
        },
        true);
    registerSensorPortInfo(sensorType, portInfo);

    registerComponent(frameProcessorComponent, [this, sensorType]() {
        auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
        auto frameProcessor = factory->createFrameProcessor(sensorType);
        return frameProcessor;
    });
}

void EGODepthNetDevice::initSensorList() {
    registerComponent(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY, [this]() {
        std::shared_ptr<FrameProcessorFactory> factory;
        TRY_EXECUTE({ factory = std::make_shared<FrameProcessorFactory>(this); })
        return factory;
    });

    // ---- Color group + single Color (each an independent RTSP stream) ----
    registerNetVideoSensor(OB_STREAM_COLOR_LEFT, OB_SENSOR_COLOR_LEFT, OB_DEV_COMPONENT_LEFT_COLOR_SENSOR, OB_DEV_COMPONENT_LEFT_COLOR_FRAME_PROCESSOR);
    registerNetVideoSensor(OB_STREAM_COLOR_RIGHT, OB_SENSOR_COLOR_RIGHT, OB_DEV_COMPONENT_RIGHT_COLOR_SENSOR, OB_DEV_COMPONENT_RIGHT_COLOR_FRAME_PROCESSOR);
    registerNetVideoSensor(OB_STREAM_COLOR_TOP, OB_SENSOR_COLOR_TOP, OB_DEV_COMPONENT_TOP_COLOR_SENSOR, OB_DEV_COMPONENT_TOP_COLOR_FRAME_PROCESSOR);
    registerNetVideoSensor(OB_STREAM_COLOR, OB_SENSOR_COLOR, OB_DEV_COMPONENT_COLOR_SENSOR, OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR);

    // ---- Depth + dual IR (output only) ----
    registerNetVideoSensor(OB_STREAM_DEPTH, OB_SENSOR_DEPTH, OB_DEV_COMPONENT_DEPTH_SENSOR, OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
    registerNetVideoSensor(OB_STREAM_IR_LEFT, OB_SENSOR_IR_LEFT, OB_DEV_COMPONENT_LEFT_IR_SENSOR, OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR);
    registerNetVideoSensor(OB_STREAM_IR_RIGHT, OB_SENSOR_IR_RIGHT, OB_DEV_COMPONENT_RIGHT_IR_SENSOR, OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR);

    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();

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

                // RTSP backend does not expose audio profiles; EGODepth network audio is PCM16 stereo,
                // 48 kHz, delivered in 2048-sample chunks.
                StreamProfileList audioProfiles;
                audioProfiles.push_back(StreamProfileFactory::createAudioStreamProfile(OB_FORMAT_PCM, 48000, 2, 16, 0, 2048));
                sensor->setStreamProfileList(audioProfiles);

                // WiFi audio carries OBNetworkFrameHeader.timestamp (device clock, same source as video/IMU).
                auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorBaseDeviceTime>(this, deviceTimeFreq_, deviceTimeFreq_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

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

        registerComponent(OB_DEV_COMPONENT_IMU_STREAMER, [this]() {
            auto sharedHidPort  = getComponentT<SharedDataStreamPort>(OB_DEV_COMPONENT_SHARED_HID_STREAM);
            auto dataStreamPort = sharedHidPort->createClientPort();
            auto imuStreamer = std::make_shared<ImuStreamer>(this, dataStreamPort, std::vector<std::shared_ptr<IFilter>>{}, std::make_shared<DMC640Decoder>());
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

        registerComponent(
            OB_DEV_COMPONENT_ACCEL_SENSOR,
            [this, imuPortInfo]() {
                auto port                 = getSourcePort(imuPortInfo);
                auto imuStreamer          = getComponentT<ImuStreamer>(OB_DEV_COMPONENT_IMU_STREAMER);
                auto imuStreamerSharedPtr = imuStreamer.get();
                auto sensor               = std::make_shared<AccelSensor>(this, port, imuStreamerSharedPtr, true);

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

                sensor->enableTimestampAnomalyDetection(false);

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_GYRO, imuPortInfo);

        registerComponent(
            OB_DEV_COMPONENT_MAG_SENSOR,
            [this, imuPortInfo]() {
                auto port                 = getSourcePort(imuPortInfo);
                auto imuStreamer          = getComponentT<ImuStreamer>(OB_DEV_COMPONENT_IMU_STREAMER);
                auto imuStreamerSharedPtr = imuStreamer.get();
                auto sensor               = std::make_shared<MagSensor>(this, port, imuStreamerSharedPtr, true);

                sensor->enableTimestampAnomalyDetection(false);

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_MAG, imuPortInfo);
    }
}

void EGODepthNetDevice::initProperties() {
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

    // color controls (broadcast to all Color sensors; firmware normalizes per-sensor)
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
    // Magnetometer counterpart of the accel/gyro properties above. MagSensor reads the current
    // odr / full-scale-range and the preset lists from these, and writes them back on stream start,
    // so they must be registered or the magnetometer stream fails with "Property not writable".
    propertyServer->registerProperty(OB_PROP_MAG_ODR_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_MAG_FULL_SCALE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_GET_MAG_PRESETS_ODR_LIST, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_GET_MAG_PRESETS_FULL_SCALE_LIST, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_MAG_SWITCH_BOOL, "", "rw", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_RAW_DATA_ALIGN_CALIB_YAML, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_IMU_CALIB_YAML, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_EXPORT_LOG, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_DEVICE_EXTENSION_INFORMATION, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_BITRATE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_DYNAMIC_BITRATE_ENABLE_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_AE_MAX_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_RAW_DATA_STREAM_PROFILE_LIST, "r", "r", vendorPropertyAccessor);
    // Depth work mode (preset) backend: the preset list / current preset reported to the user is derived
    // from these two. Matches the USB variant (EGODepthDevice::initProperties()).
    propertyServer->registerProperty(OB_RAW_DATA_DEPTH_ALG_MODE_LIST, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_CURRENT_DEPTH_ALG_MODE, "rw", "rw", vendorPropertyAccessor);
    // Effective stream profile list of the active work mode: fixSensorList() uses it to filter the profiles
    // each sensor reports. Matches the USB variant (EGODepthDevice::initProperties()).
    propertyServer->registerProperty(OB_RAW_DATA_EFFECTIVE_VIDEO_STREAM_PROFILE_LIST, "r", "r", vendorPropertyAccessor);
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
    auto customMetadataManager = std::make_shared<CustomMetadataManager>(this, vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_CUSTOM_METADATA_JSON, "w", "w", customMetadataManager);
    registerComponent(OB_DEV_COMPONENT_CUSTOM_METADATA_MANAGER, customMetadataManager);
    propertyServer->registerProperty(OB_PROP_DEVICE_VOICE_PROMPT_LANGUAGE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_WRIST_WEAR_SIDE_INT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_FW_RECORDING_PROFILE_LIST, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_FW_RECORDING_MCAP_TOPIC_LIST, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_FW_RECORDING_PART_LIST, "r", "r", vendorPropertyAccessor);

    auto imuCorrectorFilter = getSensorFrameFilter("IMUCorrector", OB_SENSOR_ACCEL);
    if(imuCorrectorFilter) {
        auto filterStateProperty = std::make_shared<FilterStatePropertyAccessor>(imuCorrectorFilter);
        propertyServer->registerProperty(OB_PROP_SDK_ACCEL_FRAME_TRANSFORMED_BOOL, "rw", "rw", filterStateProperty);
        propertyServer->registerProperty(OB_PROP_SDK_GYRO_FRAME_TRANSFORMED_BOOL, "rw", "rw", filterStateProperty);
    }

    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, false);
    registerComponent(OB_DEV_COMPONENT_FW_LOG_MANAGER, [this]() { return std::make_shared<FirmwareLogManager>(this); });
}

void EGODepthNetDevice::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {
    auto              sensorType = sensor->getSensorType();
    OBStreamType      streamType = utils::mapSensorTypeToStreamType(sensorType);
    StreamProfileList profileList;
    for(const auto &profile: allVideoStreamProfileList_) {
        if(streamType == profile->getType()) {
            profileList.push_back(profile);
        }
    }

    if(profileList.empty() && isColorStreamType(streamType)) {
        // Firmware has not yet implemented OB_RAW_DATA_STREAM_PROFILE_LIST; provide a minimal
        // hardcoded fallback so the RTSP stream can be opened for early bring-up testing.
        profileList.push_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_H264, 1600, 1200, 30));
        profileList.push_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_H264, 1600, 1200, 60));
        profileList.push_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_H265, 1600, 1200, 30));
        profileList.push_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_H265, 1600, 1200, 60));
        LOG_WARN("EGODepthNetDevice: using hardcoded fallback stream profiles for sensor {}", sensorType);
    }

    if(!profileList.empty()) {
        sensor->setStreamProfileList(profileList);
    }

    auto streamProfile = StreamProfileFactory::getDefaultStreamProfileFromEnvConfig(deviceInfo_->name_, sensorType);
    if(streamProfile) {
        sensor->updateDefaultStreamProfile(normalizeH265AliasProfile(streamProfile));
    }

    auto profiles = sensor->getStreamProfileList();
    LOG_DEBUG("Sensor {} created! Found {} stream profiles.", sensorType, profiles.size());
    for(auto &profile: profiles) {
        LOG_DEBUG(" - {}", profile);
    }
}

void EGODepthNetDevice::fetchAllVideoStreamProfileList() {
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

std::vector<std::shared_ptr<IFilter>> EGODepthNetDevice::createRecommendedPostProcessingFilters(OBSensorType type) {
    utils::unusedVar(type);
    return {};
}

// EGODepth exposes two depth work modes ("Default" / "Factory Calib") with different sensor sets.
// Mirror the USB variant (EGODepthDevice::fixSensorList()): trim the sensor/component tree for the active
// mode, then attach the effective-profile filter the firmware reports for that mode.
void EGODepthNetDevice::fixSensorList() {
    auto depthWorkModeManager = getComponentT<EGODepthDepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER, false);
    if(!depthWorkModeManager || depthWorkModeManager->getDepthWorkModeList().empty()) {
        // Firmware does not expose depth work modes: keep the full set of sensors from the enumerated ports.
        return;
    }

    const auto &currentMode = depthWorkModeManager->getCurrentDepthWorkMode();

    if(std::strcmp(currentMode.name, kEgoDepthFactoryCalibMode) == 0) {
        // Factory Calib: no Depth stream.
        deregisterSensor(OB_SENSOR_DEPTH);
        deregisterComponent(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
    }
    else {
        // Default: no left/right IR streams.
        deregisterSensor(OB_SENSOR_IR_LEFT);
        deregisterSensor(OB_SENSOR_IR_RIGHT);
        deregisterComponent(OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR);
        deregisterComponent(OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR);
        // Note: unlike the USB variant there is no IR demuxer and no OB_PROP_STOP_*_STREAM_BOOL property to
        // unregister here; every network stream owns an independent RTSP path.
    }
    // Structural trimming only, mirroring G305Device::fixSensorList(). Do NOT call getSensor() or any
    // throwing getComponentT<T>() here: fixSensorList() is reached from init(), i.e. from the constructor,
    // where IDevice::shared_from_this() is not yet usable.
}

// Instantiate the sensors that survived fixSensorList() and attach the effective stream profiles reported by
// the firmware for the active work mode. getSensor() creates components (which may pull in
// FrameProcessorFactory -> owner->shared_from_this()), so this must only run after construction completes.
void EGODepthNetDevice::applyEffectiveStreamProfileFilters() {
    auto depthWorkModeManager = getComponentT<EGODepthDepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER, false);
    if(!depthWorkModeManager || depthWorkModeManager->getDepthWorkModeList().empty()) {
        // Preserve fixSensorList()'s early-out semantics: without depth work modes nothing gets filtered.
        return;
    }

    auto propServer = getPropertyServer();

    std::vector<OBEffectiveStreamProfile> effectiveProfiles;
    bool                                  effectiveProfilesRead = false;
    BEGIN_TRY_EXECUTE({
        effectiveProfiles      = readEffectiveVideoStreamProfileList(propServer);
        effectiveProfilesRead  = true;
    })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_WARN("Failed to read effective stream profiles; keeping sensor profiles unfiltered."); })

    if(effectiveProfiles.empty()) {
        if(effectiveProfilesRead) {
            LOG_WARN("Firmware returned no effective stream profiles; keeping sensor profiles unfiltered.");
        }
        return;
    }

    for(const auto sensorType: getSensorTypeList()) {
        bool hasEffectiveProfile = false;

        for(const auto &effectiveProfile: effectiveProfiles) {
            if(effectiveProfile.sensorType == sensorType) {
                hasEffectiveProfile = true;
                break;
            }
        }
        if(!hasEffectiveProfile) {
            continue;
        }

        auto sensor = getSensor(sensorType);
        sensor->setStreamProfileFilter(std::make_shared<EGODepthEffectiveProfileFilter>(sensorType, effectiveProfiles));
    }
}

}  // namespace libobsensor
