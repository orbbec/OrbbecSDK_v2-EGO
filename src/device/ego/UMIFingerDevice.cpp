// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "UMIFingerDevice.hpp"
#include "EGOAlgParamManager.hpp"
#include "EGOVideoSensor.hpp"
#include "EGOStreamStrategy.hpp"
#include "DevicePids.hpp"
#include "environment/EnvConfig.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "sensor/imu/ImuStreamer.hpp"
#include "sensor/imu/AccelSensor.hpp"
#include "sensor/imu/GyroSensor.hpp"
#include "sensor/audio/AudioSensor.hpp"
#include "hid/SharedDataStreamPort.hpp"
#include "hid/EgoStateService.hpp"
#include "usb/uvc/UvcDevicePort.hpp"

#include "metadata/FrameMetadataParserContainer.hpp"
#include "timestamp/FrameTimestampCalculator.hpp"
#include "timestamp/DeviceClockSynchronizer.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "property/UvcPropertyAccessor.hpp"
#include "property/PropertyServer.hpp"
#include "property/CommonPropertyAccessors.hpp"
#include "property/FilterPropertyAccessors.hpp"
#include "param/AlgParamManager.hpp"
#include "component/fwlog/FirmwareLogManager.hpp"
#include "component/fwrecord/FwRecordProfileManager.hpp"
#include "component/fwrecord/FwRecordingDirectoryManager.hpp"
#include "common/DeviceSeriesInfo.hpp"

#include "FilterFactory.hpp"
#include "publicfilters/FormatConverterProcess.hpp"
#include "publicfilters/IMUCorrector.hpp"

#include "utils/PublicTypeHelper.hpp"
#include "utils/Utils.hpp"

#include "monitor/DeviceMonitor.hpp"
#include "ISourcePort.hpp"
#include "IProperty.hpp"

namespace libobsensor {

namespace {
std::shared_ptr<const StreamProfile> normalizeH265AliasProfile(const std::shared_ptr<const StreamProfile> &profile) {
    if(profile && profile->getFormat() == OB_FORMAT_HEVC) {
        return profile->clone(OB_FORMAT_H265);
    }
    return profile;
}
}  // namespace

// UMI exposes independent UVC streaming interfaces (Color / SLAM-Left / SLAM-Right), 1:1 endpoint -> stream.
// infIndex is the VideoControl interface number (= the composite MI number of each video function): the USB
// enumerator keeps only the VC interface (subclass 1) of each video function and drops the VideoStreaming
// interface (see UsbEnumeratorLibusb). Per the 0x1202 (bcdDevice 0x0310) firmware descriptor the composite
// layout is SLAM-Left=MI_00 (IF0+IF1, 1280x960), SLAM-Right=MI_02 (IF2+IF3, 1280x960), Color=MI_04
// (IF4+IF5, 1600x1200), HID/IMU=MI_06; there is no UAC audio interface in this firmware revision.
static const uint8_t UMI_USB_INTERFACE_COLOR    = 4;
static const uint8_t UMI_USB_INTERFACE_IR_LEFT  = 0;
static const uint8_t UMI_USB_INTERFACE_IR_RIGHT = 2;

UMIFingerDevice::UMIFingerDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
    init();

    // check and start heartbeat after initialization is complete
    checkAndStartHeartbeat();
}

UMIFingerDevice::~UMIFingerDevice() noexcept {}

void UMIFingerDevice::init() {
    initSensorList();
    initProperties();

    fetchDeviceInfo();
    fetchExtensionInfo();

    deviceTimeFreq_ = 1000000;
    frameTimeFreq_  = 1000000;

    auto deviceClockSynchronizer = std::make_shared<DeviceClockSynchronizer>(this, deviceTimeFreq_, deviceTimeFreq_);
    registerComponent(OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER, deviceClockSynchronizer);
    registerComponent(OB_DEV_COMPONENT_FW_RECORD_PROFILE_MANAGER, [this]() { return std::make_shared<FwRecordProfileManager>(this); });
}

void UMIFingerDevice::fetchDeviceInfo() {
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
        // UMI has no depth sensor, so try candidates in priority order.
        {
            const OBSensorType kConnTypeCandidates[] = { OB_SENSOR_COLOR, OB_SENSOR_IR_LEFT, OB_SENSOR_IR_RIGHT };
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

#ifdef __linux__
    // todo: net device does not have backend type
    // Try each candidate in priority order; UMI has no depth sensor.
    const OBSensorType kBackendCandidates[] = { OB_SENSOR_COLOR, OB_SENSOR_IR_LEFT, OB_SENSOR_IR_RIGHT };
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

void UMIFingerDevice::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {
    auto sensorType    = sensor->getSensorType();
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

void UMIFingerDevice::initSensorList() {
    registerComponent(OB_DEV_COMPONENT_SENSOR_STREAM_STRATEGY, [this]() { return std::make_shared<EGOStreamStrategy>(this); });

    registerComponent(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY, [this]() {
        std::shared_ptr<FrameProcessorFactory> factory;
        TRY_EXECUTE({ factory = std::make_shared<FrameProcessorFactory>(this); })
        return factory;
    });

    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();

    // ---- Color (UVC) ----
    auto colorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == UMI_USB_INTERFACE_COLOR;
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

        // UMI has no depth interface; the vendor XU/property channel rides on the color UVC port.
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
            auto devMonitor = std::make_shared<DeviceMonitor>(this, port);
            return devMonitor;
        });
    }

    // ---- IR Left (UVC) ----
    auto irLeftPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == UMI_USB_INTERFACE_IR_LEFT;
    });

    if(irLeftPortInfoIter != sourcePortInfoList.end()) {
        auto irLeftPortInfo = *irLeftPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_LEFT_IR_SENSOR,
            [this, irLeftPortInfo]() {
                auto port = getSourcePort(irLeftPortInfo);
                // Send the left-IR stop property through the left-IR UVC interface.
                auto sensor = std::make_shared<EGOVideoSensor>(this, OB_SENSOR_IR_LEFT, port, OB_PROP_STOP_IR_STREAM_BOOL);

                sensor->enableTimestampAnomalyDetection(false);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REPLACE, OB_FORMAT_HEVC, OB_FORMAT_H265, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGR, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGRA, OB_FORMAT_ANY, nullptr },
                };
                sensor->updateFormatFilterConfig(formatFilterConfigs);

                auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorOverUvcSCR>(this, frameTimeFreq_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_IR_LEFT, irLeftPortInfo);

        registerComponent(OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_IR_LEFT);
            return frameProcessor;
        });
    }

    // ---- IR Right (UVC) ----
    auto irRightPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC
               && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == UMI_USB_INTERFACE_IR_RIGHT;
    });

    if(irRightPortInfoIter != sourcePortInfoList.end()) {
        auto irRightPortInfo = *irRightPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_RIGHT_IR_SENSOR,
            [this, irRightPortInfo]() {
                auto port = getSourcePort(irRightPortInfo);
                // Send the right-IR stop property through the right-IR UVC interface.
                auto sensor = std::make_shared<EGOVideoSensor>(this, OB_SENSOR_IR_RIGHT, port, OB_PROP_STOP_IR_RIGHT_STREAM_BOOL);

                sensor->enableTimestampAnomalyDetection(false);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REPLACE, OB_FORMAT_HEVC, OB_FORMAT_H265, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGR, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGRA, OB_FORMAT_ANY, nullptr },
                };
                sensor->updateFormatFilterConfig(formatFilterConfigs);

                auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorOverUvcSCR>(this, frameTimeFreq_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_IR_RIGHT, irRightPortInfo);

        registerComponent(OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_IR_RIGHT);
            return frameProcessor;
        });
    }

    // ---- Audio (USB UAC microphone) ----
    auto audioPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(),
                                          [](const std::shared_ptr<const SourcePortInfo> &portInfo) { return portInfo->portType == SOURCE_PORT_USB_UAC; });

    if(audioPortInfoIter != sourcePortInfoList.end()) {
        auto audioPortInfo = *audioPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_AUDIO_SENSOR,
            [this, audioPortInfo]() {
                auto port   = getSourcePort(audioPortInfo);
                auto sensor = std::make_shared<AudioSensor>(this, port);

                // USB audio timestamps are host wall-clock (UAC has no device clock); no device-time calculator.
                sensor->enableTimestampAnomalyDetection(false);
                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_AUDIO, audioPortInfo);
    }

    // ---- IMU (HID / vendor channel) ----
    // UMI reuses the Ego accel/gyro HID link (reportId=1). DirectionKeyService is intentionally NOT registered
    // (three-camera product has no direction keys). Magnetometer (reportId=5) is not yet implemented.
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
            auto imuStreamer =
                std::make_shared<ImuStreamer>(this, dataStreamPort, std::vector<std::shared_ptr<IFilter>>{}, std::make_shared<ImuCalculatorBMI088>());
            return imuStreamer;
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

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_GYRO, imuPortInfo);
    }
}

void UMIFingerDevice::initProperties() {
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
        else if(sensor == OB_SENSOR_IR_LEFT){
            auto uvcPropertyAccessor = std::make_shared<LazyPropertyAccessor>([this, &sourcePortInfo](){
                auto port     = getSourcePort(sourcePortInfo);
                auto accessor = std::make_shared<UvcPropertyAccessor>(port);
                return accessor;
            });

            propertyServer->registerProperty(OB_PROP_IR_AUTO_EXPOSURE_BOOL, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_IR_GAIN_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_IR_BRIGHTNESS_INT, "rw", "rw", uvcPropertyAccessor);
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
            // Firmware assigns bUnitID=2 to the Orbbec vendor XU; update before creating the accessor
            // so that Linux/libuvc (which routes by unit number) targets the correct unit.
            auto uvcPort = std::dynamic_pointer_cast<UvcDevicePort>(port);
            if(uvcPort) {
                uvcPort->updateXuUnit(OB_EGO_XU_UNIT);
            }
            auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
            return accessor;
        });

        propertyServer->registerProperty(OB_PROP_COLOR_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
        // Stream stop is endpoint-local: each UMI video sensor sends its matching stop property on
        // its own interface XU, so stop properties are not writable through this global accessor.
        propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_DEVICE_SERIAL_NUMBER, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_DEVICE_TIME, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_GET_TF_CARD_CAPACITY, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_DEVICE_TEMPERATURE, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_REBOOT_DEVICE_BOOL, "w", "w", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_RECORDING_ACTION_INT, "rw", "w", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_DEVICE_IN_RECOVERY_MODE_BOOL, "w", "w", vendorPropertyAccessor);
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
        propertyServer->registerProperty(OB_PROP_VIDEO_GOP_INT, "rw", "rw", vendorPropertyAccessor);
        auto recordingDirectoryAccessor = std::make_shared<FwRecordingDirectoryManager>(this, vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_FW_RECORDING_DIRECTORY, "rw", "rw", recordingDirectoryAccessor);
        propertyServer->registerProperty(OB_PROP_DEVICE_VOICE_PROMPT_LANGUAGE_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_RAW_DATA_FW_RECORDING_PROFILE_LIST, "", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_RAW_DATA_FW_RECORDING_MCAP_TOPIC_LIST, "rw", "rw", vendorPropertyAccessor);
        //  Transparent YAML passthrough interfaces for production calibration.
        propertyServer->registerProperty(OB_RAW_DATA_ALIGN_CALIB_YAML, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_RAW_DATA_IMU_CALIB_YAML, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_RAW_DATA_EXPORT_LOG, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_MJPEG_QUALITY_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_COLOR_BITRATE_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_COLOR_DYNAMIC_BITRATE_ENABLE_BOOL, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_COLOR_DYNAMIC_BITRATE_MAX_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_COLOR_AE_MAX_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);

        auto heartbeatPropertyAccessor = std::make_shared<HeartbeatPropertyAccessor>(this);
        propertyServer->registerProperty(OB_PROP_HEARTBEAT_BOOL, "rw", "rw", heartbeatPropertyAccessor);
    }
    // Device-level vendor properties ride on the vendor XU of the ir UVC port (the main accessor).
    bool hasIr = std::find(sensors.begin(), sensors.end(), OB_SENSOR_IR_LEFT) != sensors.end();
    if(hasIr){
        auto vendorPropertyAccessor = std::make_shared<LazySuperPropertyAccessor>([this]() {
            auto &portInfo = getSensorPortInfo(OB_SENSOR_IR_LEFT);
            auto  port     = getSourcePort(portInfo);
            // Firmware assigns bUnitID=2 to the Orbbec vendor XU; update before creating the accessor
            // so that Linux/libuvc (which routes by unit number) targets the correct unit.
            auto uvcPort = std::dynamic_pointer_cast<UvcDevicePort>(port);
            if(uvcPort) {
                uvcPort->updateXuUnit(OB_EGO_XU_UNIT);
            }
            auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
            return accessor;
        });

        propertyServer->registerProperty(OB_PROP_IR_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
    }

    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, false);
    registerComponent(OB_DEV_COMPONENT_FW_LOG_MANAGER, [this]() { return std::make_shared<FirmwareLogManager>(this); });

    if(propertyServer->isPropertySupported(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        propertyServer->setPropertyValueT(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, OB_COMM_USB);
    }
}

std::vector<std::shared_ptr<IFilter>> UMIFingerDevice::createRecommendedPostProcessingFilters(OBSensorType type) {
    // UMI color/IR streams have no recommended post-processing filters by default.
    utils::unusedVar(type);
    return {};
}

}  // namespace libobsensor
