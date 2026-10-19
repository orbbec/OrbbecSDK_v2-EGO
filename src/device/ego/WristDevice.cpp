// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "WristDevice.hpp"
#include "DataAcquisitionAlgParamManager.hpp"
#include "EGOVideoSensor.hpp"
#include "EGOStreamStrategy.hpp"
#include "common/DevicePids.hpp"
#include "environment/EnvConfig.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "sensor/imu/ImuStreamer.hpp"
#include "sensor/imu/AccelSensor.hpp"
#include "sensor/imu/GyroSensor.hpp"
#include "hid/SharedDataStreamPort.hpp"
#include "hid/EgoStateService.hpp"
#include "hid/DirectionKeyService.hpp"
#include "usb/uvc/UvcDevicePort.hpp"

#include "metadata/FrameMetadataParserContainer.hpp"
#include "timestamp/FrameTimestampCalculator.hpp"
#include "timestamp/DeviceClockSynchronizer.hpp"
#include "syncconfig/DeviceSyncConfigurator.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "property/UvcPropertyAccessor.hpp"
#include "property/PropertyServer.hpp"
#include "property/CommonPropertyAccessors.hpp"
#include "property/FilterPropertyAccessors.hpp"
#include "param/AlgParamManager.hpp"
#include "component/fwrecord/FwRecordProfileManager.hpp"
#include "component/fwrecord/FwRecordingDirectoryManager.hpp"
#include "component/fwrecord/RecordingSessionResultManager.hpp"
#include "component/fwlog/FirmwareLogManager.hpp"
#include "component/custommetadata/CustomMetadataManager.hpp"
#include "component/timezone/HostTimeInfoManager.hpp"

#include "FilterFactory.hpp"
#include "publicfilters/FormatConverterProcess.hpp"
#include "publicfilters/IMUCorrector.hpp"

#include "utils/PublicTypeHelper.hpp"
#include "utils/Utils.hpp"

#include "monitor/EgoDeviceMonitor.hpp"
#include "ISourcePort.hpp"
#include "IProperty.hpp"

#include <algorithm>

namespace libobsensor {

namespace {
std::shared_ptr<const StreamProfile> normalizeH265AliasProfile(const std::shared_ptr<const StreamProfile> &profile) {
    if(profile && profile->getFormat() == OB_FORMAT_HEVC) {
        return profile->clone(OB_FORMAT_H265);
    }
    return profile;
}

}  // namespace

static const uint8_t WRIST_USB_INTERFACE_COLOR = 0;
static const std::vector<OBMultiDeviceSyncMode> wristSupportedSyncModes = {
    OB_MULTI_DEVICE_SYNC_MODE_PRIMARY,
    OB_MULTI_DEVICE_SYNC_MODE_SECONDARY,
};

WristDevice::WristDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
    init();
}

WristDevice::~WristDevice() noexcept {}

void WristDevice::init() {
    initSensorList();
    initProperties();

    fetchDeviceInfo();
    {
        auto propertyServer = getPropertyServer();
        auto vendorPropertyAccessor = getComponentT<VendorPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        auto fwRecordingTextPropertyAccessor = std::make_shared<FwRecordingDirectoryManager>(this, vendorPropertyAccessor.get());
        propertyServer->registerProperty(OB_STRUCT_FW_RECORDING_DIRECTORY, "rw", "rw", fwRecordingTextPropertyAccessor);
        if(getFirmwareVersionInt() >= 10108) {  // Firmware 1.1.8 and newer
            propertyServer->registerProperty(OB_STRUCT_FW_RECORDING_FILE_NAME, "rw", "rw", fwRecordingTextPropertyAccessor);
        }
    }
    registerRecoveryModeProperty();
    fetchExtensionInfo();

    if(getFirmwareVersionInt() >= 10109) {
        auto algParamManager = std::make_shared<DataAcquisitionAlgParamManager>(this);
        registerComponent(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, algParamManager);
    }
    
    deviceTimeFreq_ = 1000000;
    frameTimeFreq_  = 1000000;
    auto deviceClockSynchronizer = std::make_shared<DeviceClockSynchronizer>(this, deviceTimeFreq_, deviceTimeFreq_);
    registerComponent(OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER, deviceClockSynchronizer);
    registerComponent(OB_DEV_COMPONENT_FW_RECORD_PROFILE_MANAGER, [this]() { return std::make_shared<FwRecordProfileManager>(this); });

    auto hostTimeInfoManager = std::make_shared<HostTimeInfoManager>(this);
    registerComponent(OB_DEV_COMPONENT_HOST_TIME_INFO_MANAGER, hostTimeInfoManager);
    BEGIN_TRY_EXECUTE({
        if(getFirmwareVersionInt() > 10006) {
            hostTimeInfoManager->setHostTimeInfoToDevice();
        }
        else {
            hostTimeInfoManager->setUtcOffsetToDevice();
        }
    })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_WARN("Failed to initialize host time information for Wrist device"); })

    {
        auto propertyServer         = getPropertyServer();
        auto vendorPropertyAccessor = getComponentT<VendorPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        propertyServer->registerProperty(OB_STRUCT_DEVICE_MODEL_TYPE, "r", "r", vendorPropertyAccessor.get());
    }

    if(getFirmwareVersionInt() > 10102) {
        auto propertyServer = getPropertyServer();
        auto vendorAccessor = getComponentT<VendorPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        propertyServer->registerProperty(OB_STRUCT_EGO_STATE_REPORT, "r", "r", vendorAccessor.get());
        propertyServer->registerProperty(OB_STRUCT_RECORDING_SESSION_RESULT, "", "rw", vendorAccessor.get());
        registerComponent(OB_DEV_COMPONENT_RECORDING_SESSION_RESULT_MANAGER,
                          std::make_shared<RecordingSessionResultManager>(this, vendorAccessor.get()));
    }

    if(getFirmwareVersionInt() >= 10109) {
        auto propertyServer = getPropertyServer();
        auto vendorAccessor = getComponentT<VendorPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        propertyServer->registerProperty(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, "rw", "rw", vendorAccessor.get());
        registerComponent(OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR, std::make_shared<DeviceSyncConfigurator>(this, wristSupportedSyncModes));
    }
}

void WristDevice::fetchDeviceInfo() {
    auto propServer = getPropertyServer();

    auto version                      = propServer->getStructureDataT<OBVersionInfo>(OB_STRUCT_VERSION);
    deviceInfo_->name_                = version.deviceName;
    deviceInfo_->fwVersion_           = version.firmwareVersion;
    deviceInfo_->deviceSn_            = version.serialNumber;
    deviceInfo_->asicName_            = version.depthChip;
    deviceInfo_->hwVersion_           = version.hardwareVersion;
    deviceInfo_->type_                = static_cast<uint16_t>(version.deviceType);
    deviceInfo_->supportedSdkVersion_ = version.sdkVersion;
    deviceInfo_->subSystemVersion_    = version.subSystemVersion;

#ifdef __ANDROID__
    // Android need to query usb connect type from port.
    // Wrist has no depth sensor, so try candidates in priority order.
    {
        const OBSensorType kConnTypeCandidates[] = { OB_SENSOR_DEPTH, OB_SENSOR_COLOR, OB_SENSOR_IR };
        for(auto candidateType: kConnTypeCandidates) {
            if(!isSensorExists(candidateType)) {
                continue;
            }
            auto portInfo   = getSensorPortInfo(candidateType);
            auto sourcePort = getSourcePort(portInfo);
            auto uvcDevPort = std::dynamic_pointer_cast<UvcDevicePort>(sourcePort);
            if(uvcDevPort) {
                deviceInfo_->connectionType_ = uvcDevPort->getUsbConnectType();
                break;
            }
        }
    }
#endif

    // remove the prefix "Orbbec " from the device name if contained
    if(deviceInfo_->name_.find("Orbbec ") == 0) {
        deviceInfo_->name_ = deviceInfo_->name_.substr(7);
    }
    deviceInfo_->fullName_ = deviceInfo_->name_;

    const size_t deviceUserNameBufferSize = 32;
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

#ifdef __linux__
    // todo: net device does not have backend type
    // Try each candidate in priority order; Wrist has no depth sensor.
    const OBSensorType kBackendCandidates[] = { OB_SENSOR_DEPTH, OB_SENSOR_COLOR, OB_SENSOR_IR };
    for(auto candidateType: kBackendCandidates) {
        if(!isSensorExists(candidateType)) {
            continue;
        }
        auto sensorPortInfo = getSensorPortInfo(candidateType);
        if(sensorPortInfo->portType == SOURCE_PORT_USB_UVC) {
            auto port                 = getSourcePort(sensorPortInfo);
            auto uvcPort              = std::dynamic_pointer_cast<UvcDevicePort>(port);
            deviceInfo_->backendType_ = uvcPort->getBackendType();
            break;
        }
    }
#else
    deviceInfo_->backendType_ = OB_UVC_BACKEND_TYPE_MSMF;
#endif

    // mark the device as a multi-sensor device with same clock at default
    extensionInfo_["AllSensorsUsingSameClock"] = "true";
}

void WristDevice::registerRecoveryModeProperty() {
    auto propertyServer  = getPropertyServer();
    auto vendorAccessor  = getComponentT<VendorPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
    auto firmwareVersion = getFirmwareVersionInt();

    const auto devicePropertyId = firmwareVersion >= 10006
                                      ? OB_PROP_BOOT_INTO_RECOVERY_MODE_BOOL
                                      : OB_PROP_DEVICE_IN_RECOVERY_MODE_BOOL;
    auto mappedAccessor = std::make_shared<PropertyIdMappingAccessor>(vendorAccessor.get(), devicePropertyId);

    // The SDK always exposes the correct public property. Firmware before 1.0.6
    // implemented the command under the legacy property ID internally.
    propertyServer->registerProperty(OB_PROP_BOOT_INTO_RECOVERY_MODE_BOOL, "w", "w", mappedAccessor);
}

void WristDevice::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {
    auto sensorType    = sensor->getSensorType();
    auto streamProfile = StreamProfileFactory::getDefaultStreamProfileFromEnvConfig(deviceInfo_->name_, sensorType);
    if(streamProfile) {
        sensor->updateDefaultStreamProfile(normalizeH265AliasProfile(streamProfile));
    }

    auto profiles = sensor->getStreamProfileList();
    if(sensorType != OB_SENSOR_AUDIO) {
        auto algParamManager = getComponentT<DataAcquisitionAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, false);
        if(algParamManager) {
            algParamManager->bindStreamProfileParams(profiles);
        }
    }
    LOG_DEBUG("Sensor {} created! Found {} stream profiles.", sensorType, profiles.size());
    for(auto &profile: profiles) {
        LOG_DEBUG(" - {}", profile);
    }
}

void WristDevice::initSensorList() {
    registerComponent(OB_DEV_COMPONENT_SENSOR_STREAM_STRATEGY, [this]() { return std::make_shared<EGOStreamStrategy>(this); });

    registerComponent(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY, [this]() {
        std::shared_ptr<FrameProcessorFactory> factory;
        TRY_EXECUTE({ factory = std::make_shared<FrameProcessorFactory>(this); })
        return factory;
    });

    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();

    // ---- Color (UVC) ----
    auto colorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC
               && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == WRIST_USB_INTERFACE_COLOR;
    });

    if(colorPortInfoIter != sourcePortInfoList.end()) {
        auto colorPortInfo = *colorPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_COLOR_SENSOR,
            [this, colorPortInfo]() {
                auto port   = getSourcePort(colorPortInfo);
                auto sensor = std::make_shared<EGOVideoSensor>(this, OB_SENSOR_COLOR, port, OB_PROP_STOP_COLOR_STREAM_BOOL);

                sensor->enableTimestampAnomalyDetection(false);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REPLACE, OB_FORMAT_HEVC, OB_FORMAT_H265, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGR, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGRA, OB_FORMAT_ANY, nullptr },
                };
                auto formatConverter = getSensorFrameFilter("FormatConverter", OB_SENSOR_COLOR, false);
                sensor->updateFormatFilterConfig(formatFilterConfigs);

                auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorOverUvcSCR>(this, frameTimeFreq_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

                // auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
                // sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_COLOR, colorPortInfo);

        registerComponent(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_COLOR);
            return frameProcessor;
        });

        // Wrist has no depth interface; the vendor XU/property channel rides on the color UVC port.
        registerComponent(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR, [this, colorPortInfo]() {
            auto port    = getSourcePort(colorPortInfo);
            auto uvcPort = std::dynamic_pointer_cast<UvcDevicePort>(port);
            if(uvcPort) {
                uvcPort->updateXuUnit(OB_EGO_XU_UNIT);
            }
            auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
            return accessor;
        });

        registerComponent(OB_DEV_COMPONENT_DEVICE_MONITOR, [this, colorPortInfo]() {
            auto port    = getSourcePort(colorPortInfo);
            auto uvcPort = std::dynamic_pointer_cast<UvcDevicePort>(port);
            if(uvcPort) {
                uvcPort->updateXuUnit(OB_EGO_XU_UNIT);
            }
            auto devMonitor = std::make_shared<EgoDeviceMonitor>(this, port);
            return devMonitor;
        });
    }

    // ---- IMU (HID / vendor channel) ----
    auto imuPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(),
                                        [](const std::shared_ptr<const SourcePortInfo> &portInfo) { return portInfo->portType == SOURCE_PORT_USB_HID; });

    if(imuPortInfoIter != sourcePortInfoList.end()) {
        auto imuPortInfo = *imuPortInfoIter;
        registerComponent(OB_DEV_COMPONENT_SHARED_HID_STREAM, [this, imuPortInfo]() {
            auto port           = getSourcePort(imuPortInfo);
            auto dataStreamPort = std::dynamic_pointer_cast<IDataStreamPort>(port);
            auto sharedHidPort  = std::make_shared<SharedDataStreamPort>(this, dataStreamPort);
            return sharedHidPort;
        });

        registerComponent(OB_DEV_COMPONENT_IMU_STREAMER, [this]() {
            // the gyro and accel are both on the same port; no correction filter applied
            auto sharedHidPort  = getComponentT<SharedDataStreamPort>(OB_DEV_COMPONENT_SHARED_HID_STREAM);
            auto dataStreamPort = sharedHidPort->createClientPort();
            auto imuStreamer = std::make_shared<ImuStreamer>(this, dataStreamPort, std::vector<std::shared_ptr<IFilter>>{},
                                                              std::make_shared<ImuCalculatorBMI088>());
            return imuStreamer;
        });

        registerComponent(OB_DEV_COMPONENT_DIRECTION_KEY_SERVICE, [this]() {
            auto sharedHidPort = getComponentT<SharedDataStreamPort>(OB_DEV_COMPONENT_SHARED_HID_STREAM);
            return std::make_shared<DirectionKeyService>(this, sharedHidPort->createClientPort());
        });

        registerComponent(OB_DEV_COMPONENT_EGO_STATE_SERVICE, [this]() {
            auto sharedHidPort = getComponentT<SharedDataStreamPort>(OB_DEV_COMPONENT_SHARED_HID_STREAM);
            return std::make_shared<EgoStateService>(this, sharedHidPort->createClientPort());
        });

        registerComponent(
            OB_DEV_COMPONENT_ACCEL_SENSOR,
            [this, imuPortInfo]() {
                auto port                 = getSourcePort(imuPortInfo);
                auto imuStreamer          = getComponentT<ImuStreamer>(OB_DEV_COMPONENT_IMU_STREAMER);
                auto imuStreamerSharedPtr = imuStreamer.get();
                auto sensor               = std::make_shared<AccelSensor>(this, port, imuStreamerSharedPtr, true);

                sensor->enableTimestampAnomalyDetection(false);

                // Firmware IMU timestamp unit is microseconds (1MHz); pass through directly.
                auto imuFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorDirectly>(this, frameTimeFreq_);
                sensor->setFrameTimestampCalculator(imuFrameTimestampCalculator);

                // auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
                // sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

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

                // Firmware IMU timestamp unit is microseconds (1MHz); pass through directly.
                auto imuFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorDirectly>(this, frameTimeFreq_);
                sensor->setFrameTimestampCalculator(imuFrameTimestampCalculator);

                // auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
                // sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_GYRO, imuPortInfo);
    }
}

void WristDevice::initProperties() {
    auto propertyServer = std::make_shared<PropertyServer>(this);

    auto sensors = getSensorTypeList();
    for(auto &sensor: sensors) {
        auto &sourcePortInfo = getSensorPortInfo(sensor);
        if(sensor == OB_SENSOR_COLOR) {
            auto uvcPropertyAccessor = std::make_shared<LazyPropertyAccessor>([this, &sourcePortInfo]() {
                auto port     = getSourcePort(sourcePortInfo);
                auto accessor = std::make_shared<UvcPropertyAccessor>(port);
                return accessor;
            });

            propertyServer->registerProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_GAIN_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_SATURATION_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_WHITE_BALANCE_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_BRIGHTNESS_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_SHARPNESS_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_CONTRAST_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, "rw", "rw", uvcPropertyAccessor);
        }
        else if(sensor == OB_SENSOR_ACCEL) {
            auto imuCorrectorFilter = getSensorFrameFilter("IMUCorrector", sensor);
            if(imuCorrectorFilter) {
                auto filterStateProperty = std::make_shared<FilterStatePropertyAccessor>(imuCorrectorFilter);
                propertyServer->registerProperty(OB_PROP_SDK_ACCEL_FRAME_TRANSFORMED_BOOL, "rw", "rw", filterStateProperty);
            }
        }
        else if(sensor == OB_SENSOR_GYRO) {
            auto imuCorrectorFilter = getSensorFrameFilter("IMUCorrector", sensor);
            if(imuCorrectorFilter) {
                auto filterStateProperty = std::make_shared<FilterStatePropertyAccessor>(imuCorrectorFilter);
                propertyServer->registerProperty(OB_PROP_SDK_GYRO_FRAME_TRANSFORMED_BOOL, "rw", "rw", filterStateProperty);
            }
        }
    }

    // Device-level vendor properties ride on the vendor XU of the color UVC port (the main accessor).
    bool hasColor = std::find(sensors.begin(), sensors.end(), OB_SENSOR_COLOR) != sensors.end();
    if(hasColor) {
        auto vendorPropertyAccessor = std::make_shared<LazySuperPropertyAccessor>([this]() {
            auto &portInfo = getSensorPortInfo(OB_SENSOR_COLOR);
            auto  port     = getSourcePort(portInfo);
            // Wrist firmware assigns bUnitID=2 to the Orbbec vendor XU; update before creating the accessor
            // so that Linux/libuvc (which routes by unit number) targets the correct unit.
            auto uvcPort = std::dynamic_pointer_cast<UvcDevicePort>(port);
            if(uvcPort) {
                uvcPort->updateXuUnit(OB_EGO_XU_UNIT);
            }
            auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
            return accessor;
        });

        propertyServer->registerProperty(OB_PROP_COLOR_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_LOCAL_UTC_OFFSET_MINUTES_INT, "", "w", vendorPropertyAccessor);
        // WristColorVideoSensor sends OB_PROP_STOP_COLOR_STREAM_BOOL on the color
        // sensor's own interface XU, so it is intentionally NOT registered on this device-global accessor.
        // TODO:
        propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_HOST_TIME_INFO, "", "w", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_DEVICE_SERIAL_NUMBER, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_DEVICE_TIME, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_GET_TF_CARD_CAPACITY, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_DEVICE_TEMPERATURE, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_REBOOT_DEVICE_BOOL, "w", "w", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_RECORDING_ACTION_INT, "rw", "w", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_GYRO_ODR_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_ACCEL_ODR_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_GYRO_FULL_SCALE_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_ACCEL_FULL_SCALE_INT, "rw", "rw", vendorPropertyAccessor);
        // IMU preset lists: AccelSensor/GyroSensor build their supported stream-profile list from these, so
        // they must be registered or the IMU stream start fails with "profile is not supported".
        propertyServer->registerProperty(OB_STRUCT_GET_ACCEL_PRESETS_ODR_LIST, "", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_GET_ACCEL_PRESETS_FULL_SCALE_LIST, "", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_GET_GYRO_PRESETS_ODR_LIST, "", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_GET_GYRO_PRESETS_FULL_SCALE_LIST, "", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_ACCEL_SWITCH_BOOL, "", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_GYRO_SWITCH_BOOL, "", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_FW_RECORDING_STATE_BOOL, "", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_FW_RECORDING_FILE_SPLIT_DURATION_MIN_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_RECORDING_DESTINATION_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_FW_RECORDING_FORMAT_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_SD_CARD_RECORDING_ENCRYPTION_ENABLE_BOOL, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_RAW_DATA_SD_CARD_RECORDING_ENCRYPTION_PUBLIC_KEY, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_VIDEO_GOP_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_DEVICE_USER_NAME, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_BLUETOOTH_PAIRING_INFO, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_WIFI_CONFIG, "rw", "rw", vendorPropertyAccessor);
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
        // propertyServer->registerProperty(OB_RAW_DATA_IMU_CALIB_PARAM, "", "rw", vendorPropertyAccessor);
        // propertyServer->registerProperty(OB_RAW_DATA_ALIGN_CALIB_PARAM, "", "r", vendorPropertyAccessor);
        //  Transparent YAML passthrough interfaces for production calibration.
        //  The firmware writes YAML files to flash during calibration; these properties read them back as
        //  raw bytes. The SDK does not parse the data; the application layer saves it directly to a file.
        propertyServer->registerProperty(OB_RAW_DATA_ALIGN_CALIB_YAML, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_RAW_DATA_IMU_CALIB_YAML, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_RAW_DATA_CONVERT_PARAMS, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_RAW_DATA_EXPORT_LOG, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_COLOR_BITRATE_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_COLOR_DYNAMIC_BITRATE_ENABLE_BOOL, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_COLOR_DYNAMIC_BITRATE_MAX_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_COLOR_AE_MAX_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);

    }

    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, false);
    registerComponent(OB_DEV_COMPONENT_FW_LOG_MANAGER, [this]() { return std::make_shared<FirmwareLogManager>(this); });

    if(propertyServer->isPropertySupported(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        propertyServer->setPropertyValueT(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, OB_COMM_USB);
    }
}

std::vector<std::shared_ptr<IFilter>> WristDevice::createRecommendedPostProcessingFilters(OBSensorType type) {
    // Wrist color streams have no recommended post-processing filters by default.
    utils::unusedVar(type);
    return {};
}

}  // namespace libobsensor
