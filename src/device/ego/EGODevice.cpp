// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EGODevice.hpp"
#include "EGOAlgParamManager.hpp"
#include "DevicePids.hpp"
#include "environment/EnvConfig.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "sensor/imu/ImuStreamer.hpp"
#include "sensor/imu/AccelSensor.hpp"
#include "sensor/imu/GyroSensor.hpp"
#include "sensor/audio/AudioSensor.hpp"
#include "usb/uvc/UvcDevicePort.hpp"

#include "metadata/FrameMetadataParserContainer.hpp"
#include "timestamp/GlobalTimestampFitter.hpp"
#include "timestamp/FrameTimestampCalculator.hpp"
#include "timestamp/DeviceClockSynchronizer.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "property/UvcPropertyAccessor.hpp"
#include "property/PropertyServer.hpp"
#include "property/CommonPropertyAccessors.hpp"
#include "property/FilterPropertyAccessors.hpp"
#include "param/AlgParamManager.hpp"

#include "FilterFactory.hpp"
#include "publicfilters/FormatConverterProcess.hpp"
#include "publicfilters/IMUCorrector.hpp"

#include "utils/PublicTypeHelper.hpp"
#include "utils/Utils.hpp"

#include "firmwareupdater/FirmwareUpdater.hpp"
#include "monitor/DeviceMonitor.hpp"
#include "ISourcePort.hpp"
#include "IProperty.hpp"

namespace libobsensor {

// EGO USB exposes two independent RGB cameras as two UVC VideoStreaming interfaces.
// UVC interfaces are typically grouped (control + streaming) so the streaming interface
// indices come in pairs. These values are placeholders pending firmware descriptor confirmation.
static const uint8_t EGO_USB_INTERFACE_COLOR_LEFT  = 0;
static const uint8_t EGO_USB_INTERFACE_COLOR_RIGHT = 2;

EGODevice::EGODevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
    init();

    // check and start heartbeat after initialization is complete
    checkAndStartHeartbeat();
}

EGODevice::~EGODevice() noexcept {}

void EGODevice::init() {
    initSensorList();
    initProperties();

    fetchDeviceInfo();
    fetchExtensionInfo();

    deviceTimeFreq_ = 1000000;     // OB_STRUCT_DEVICE_TIME is reported in microseconds (1MHz).
    frameTimeFreq_  = 1000000000;  // Per-frame timestamp (video SCR / IMU) counts at 1GHz (nanoseconds);
                                   // the calculators divide by this freq to output microseconds.

    auto globalTimestampFilter = std::make_shared<GlobalTimestampFitter>(this);
    registerComponent(OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER, globalTimestampFilter);

    auto algParamManager = std::make_shared<EGOAlgParamManager>(this);
    registerComponent(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, algParamManager);

    auto deviceClockSynchronizer = std::make_shared<DeviceClockSynchronizer>(this, deviceTimeFreq_, deviceTimeFreq_);
    registerComponent(OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER, deviceClockSynchronizer);

    // firmware updater
    registerComponent(OB_DEV_COMPONENT_FIRMWARE_UPDATER, [this]() {
        std::shared_ptr<FirmwareUpdater> firmwareUpdater;
        TRY_EXECUTE({ firmwareUpdater = std::make_shared<FirmwareUpdater>(this); })
        return firmwareUpdater;
    });
}

void EGODevice::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {
    auto sensorType    = sensor->getSensorType();
    auto streamProfile = StreamProfileFactory::getDefaultStreamProfileFromEnvConfig(deviceInfo_->name_, sensorType);
    if(streamProfile) {
        sensor->updateDefaultStreamProfile(streamProfile);
    }

    auto profiles = sensor->getStreamProfileList();

    // bind params: extrinsics, intrinsics, etc. Only the calibrated EGO streams (dual color + IMU) are
    // bound; audio has no EGO spatial calibration.
    if(sensorType == OB_SENSOR_COLOR_LEFT || sensorType == OB_SENSOR_COLOR_RIGHT || sensorType == OB_SENSOR_ACCEL || sensorType == OB_SENSOR_GYRO) {
        auto algParamManager = getComponentT<EGOAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER);
        algParamManager->bindStreamProfileParams(profiles);
    }

    LOG_DEBUG("Sensor {} created! Found {} stream profiles.", sensorType, profiles.size());
    for(auto &profile: profiles) {
        LOG_DEBUG(" - {}", profile);
    }
}

void EGODevice::initSensorList() {
    registerComponent(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY, [this]() {
        std::shared_ptr<FrameProcessorFactory> factory;
        TRY_EXECUTE({ factory = std::make_shared<FrameProcessorFactory>(this); })
        return factory;
    });

    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();

    // ---- Left color (UVC) ----
    auto leftColorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC
               && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == EGO_USB_INTERFACE_COLOR_LEFT;
    });

    if(leftColorPortInfoIter != sourcePortInfoList.end()) {
        auto leftColorPortInfo = *leftColorPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_LEFT_COLOR_SENSOR,
            [this, leftColorPortInfo]() {
                auto port   = getSourcePort(leftColorPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_COLOR_LEFT, port);

                // [EGO-WIP] EGO firmware does not yet provide a reliable per-frame device timestamp, which
                // trips the anomaly detector and drops every frame. Disable it until the timestamp is fixed.
                sensor->enableTimestampAnomalyDetection(false);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGR, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGRA, OB_FORMAT_ANY, nullptr },
                };
                auto formatConverter = getSensorFrameFilter("FormatConverter", OB_SENSOR_COLOR_LEFT, false);
                sensor->updateFormatFilterConfig(formatFilterConfigs);

                auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorOverUvcSCR>(this, frameTimeFreq_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

                // Both color sensors share the same device clock domain via the single
                // OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER on this device, so left/right stay aligned.
                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
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

        // EGO has no depth interface; the vendor XU/property channel rides on the left color UVC port.
        registerComponent(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR, [this, leftColorPortInfo]() {
            auto port    = getSourcePort(leftColorPortInfo);
            auto uvcPort = std::dynamic_pointer_cast<UvcDevicePort>(port);
            if(uvcPort) {
                uvcPort->updateXuUnit(OB_EGO_XU_UNIT);
            }
            auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
            return accessor;
        });

        registerComponent(OB_DEV_COMPONENT_DEVICE_MONITOR, [this, leftColorPortInfo]() {
            auto port    = getSourcePort(leftColorPortInfo);
            auto uvcPort = std::dynamic_pointer_cast<UvcDevicePort>(port);
            if(uvcPort) {
                uvcPort->updateXuUnit(OB_EGO_XU_UNIT);
            }
            auto devMonitor = std::make_shared<DeviceMonitor>(this, port);
            return devMonitor;
        });
    }

    // ---- Right color (UVC) ----
    auto rightColorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC
               && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == EGO_USB_INTERFACE_COLOR_RIGHT;
    });

    if(rightColorPortInfoIter != sourcePortInfoList.end()) {
        auto rightColorPortInfo = *rightColorPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_RIGHT_COLOR_SENSOR,
            [this, rightColorPortInfo]() {
                auto port   = getSourcePort(rightColorPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_COLOR_RIGHT, port);

                // [EGO-WIP] EGO firmware does not yet provide a reliable per-frame device timestamp, which
                // trips the anomaly detector and drops every frame. Disable it until the timestamp is fixed.
                sensor->enableTimestampAnomalyDetection(false);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGR, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGRA, OB_FORMAT_ANY, nullptr },
                };
                auto formatConverter = getSensorFrameFilter("FormatConverter", OB_SENSOR_COLOR_RIGHT, false);
                sensor->updateFormatFilterConfig(formatFilterConfigs);

                auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorOverUvcSCR>(this, frameTimeFreq_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
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

    // ---- Audio (USB UAC microphone) ----
    // The AudioStreaming interface is enumerated as SOURCE_PORT_USB_UAC and grouped onto this device.
    // (On libusb platforms it appears automatically; the Windows WASAPI port is paired by VID/PID.)
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
                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_AUDIO, audioPortInfo);
    }

    // ---- IMU (HID / vendor channel) ----
    auto imuPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(),
                                        [](const std::shared_ptr<const SourcePortInfo> &portInfo) { return portInfo->portType == SOURCE_PORT_USB_HID; });

    if(imuPortInfoIter != sourcePortInfoList.end()) {
        auto imuPortInfo = *imuPortInfoIter;
        registerComponent(OB_DEV_COMPONENT_IMU_STREAMER, [this, imuPortInfo]() {
            // the gyro and accel are both on the same port and share the same filter
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

                // Firmware fills the IMU timestamp with a 1GHz (nanosecond) counter; convert it to microseconds.
                auto imuFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorDirectly>(this, frameTimeFreq_);
                sensor->setFrameTimestampCalculator(imuFrameTimestampCalculator);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
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

                // Firmware fills the IMU timestamp with a 1GHz (nanosecond) counter; convert it to microseconds.
                auto imuFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorDirectly>(this, frameTimeFreq_);
                sensor->setFrameTimestampCalculator(imuFrameTimestampCalculator);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_GYRO, imuPortInfo);
    }
}

void EGODevice::initProperties() {
    auto propertyServer = std::make_shared<PropertyServer>(this);

    auto sensors = getSensorTypeList();
    for(auto &sensor: sensors) {
        auto &sourcePortInfo = getSensorPortInfo(sensor);
        if(sensor == OB_SENSOR_COLOR_LEFT || sensor == OB_SENSOR_COLOR_RIGHT) {
            auto uvcPropertyAccessor = std::make_shared<LazyPropertyAccessor>([this, &sourcePortInfo]() {
                auto port     = getSourcePort(sourcePortInfo);
                auto accessor = std::make_shared<UvcPropertyAccessor>(port);
                return accessor;
            });

            propertyServer->registerProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_EXPOSURE_INT, "rw", "rw", uvcPropertyAccessor);
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

    // Device-level vendor properties ride on the vendor XU of the left color UVC port (the main accessor).
    bool hasLeftColor = std::find(sensors.begin(), sensors.end(), OB_SENSOR_COLOR_LEFT) != sensors.end();
    if(hasLeftColor) {
        auto vendorPropertyAccessor = std::make_shared<LazySuperPropertyAccessor>([this]() {
            auto &portInfo = getSensorPortInfo(OB_SENSOR_COLOR_LEFT);
            auto  port     = getSourcePort(portInfo);
            // EGO firmware assigns bUnitID=2 to the Orbbec vendor XU; update before creating the accessor
            // so that Linux/libuvc (which routes by unit number) targets the correct unit.
            auto uvcPort = std::dynamic_pointer_cast<UvcDevicePort>(port);
            if(uvcPort) {
                uvcPort->updateXuUnit(OB_EGO_XU_UNIT);
            }
            auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
            return accessor;
        });

        // TODO:
        propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_DEVICE_SERIAL_NUMBER, "r", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_DEVICE_TIME, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_REBOOT_DEVICE_BOOL, "w", "w", vendorPropertyAccessor);
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
        propertyServer->registerProperty(OB_RAW_DATA_IMU_CALIB_PARAM, "", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_RAW_DATA_ALIGN_CALIB_PARAM, "", "r", vendorPropertyAccessor);

        auto heartbeatPropertyAccessor = std::make_shared<HeartbeatPropertyAccessor>(this);
        propertyServer->registerProperty(OB_PROP_HEARTBEAT_BOOL, "rw", "rw", heartbeatPropertyAccessor);
    }

    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, false);

    if(propertyServer->isPropertySupported(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        propertyServer->setPropertyValueT(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, OB_COMM_USB);
    }
}

std::vector<std::shared_ptr<IFilter>> EGODevice::createRecommendedPostProcessingFilters(OBSensorType type) {
    // EGO color streams have no recommended post-processing filters by default.
    utils::unusedVar(type);
    return {};
}

}  // namespace libobsensor
