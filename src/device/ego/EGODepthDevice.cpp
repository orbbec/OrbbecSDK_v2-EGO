// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EGODepthDevice.hpp"
#include "EGODepthDepthWorkModeManager.hpp"
#include "EGODepthPresetManager.hpp"
#include "EGODepthStreamProfileFilter.hpp"
#include "EGOStreamDemuxer.hpp"
#include "EGOVideoSensor.hpp"
#include "DevicePids.hpp"
#include "environment/EnvConfig.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "sensor/imu/ImuStreamer.hpp"
#include "sensor/imu/AccelSensor.hpp"
#include "sensor/imu/GyroSensor.hpp"
#include "sensor/imu/MagSensor.hpp"
#include "sensor/imu/ImuPayloadDecoder.hpp"
#include "sensor/audio/AudioSensor.hpp"
#include "hid/SharedDataStreamPort.hpp"
#include "hid/EgoStateService.hpp"
#include "hid/BatteryInfoService.hpp"
#include "hid/DirectionKeyService.hpp"
#include "usb/uvc/UvcDevicePort.hpp"

#include "metadata/FrameMetadataParserContainer.hpp"
#include "timestamp/FrameTimestampCalculator.hpp"
#include "timestamp/DeviceClockSynchronizer.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "property/UvcPropertyAccessor.hpp"
#include "property/PropertyServer.hpp"
#include "property/CommonPropertyAccessors.hpp"
#include "property/FilterPropertyAccessors.hpp"
#include "component/property/InternalProperty.hpp"
#include "component/fwlog/FirmwareLogManager.hpp"
#include "component/fwrecord/FwRecordProfileManager.hpp"
#include "component/fwrecord/FwRecordingDirectoryManager.hpp"
#include "component/custommetadata/CustomMetadataManager.hpp"
#include "component/timezone/HostTimeInfoManager.hpp"
#include "common/DeviceSeriesInfo.hpp"

#include "FilterFactory.hpp"
#include "publicfilters/FormatConverterProcess.hpp"
#include "publicfilters/IMUCorrector.hpp"

#include "utils/PublicTypeHelper.hpp"
#include "utils/Utils.hpp"

#include "monitor/DeviceMonitor.hpp"
#include "ISourcePort.hpp"
#include "IProperty.hpp"

#include <tuple>
#include <vector>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <utility>

namespace libobsensor {

namespace {
std::shared_ptr<const StreamProfile> normalizeH265AliasProfile(const std::shared_ptr<const StreamProfile> &profile) {
    if(profile && profile->getFormat() == OB_FORMAT_HEVC) {
        return profile->clone(OB_FORMAT_H265);
    }
    return profile;
}

// The firmware switches this Y16 endpoint between depth and raw disparity.
// Keep the host-side frame semantic in sync without adding a software D2D stage.
class EGODepthD2DSensor : public VideoSensor {
public:
    EGODepthD2DSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend) : VideoSensor(owner, OB_SENSOR_DEPTH, backend) {}

    void markOutputDisparityFrame(bool enable) {
        outputDisparityFrame_.store(enable, std::memory_order_relaxed);
    }

protected:
    void outputFrame(std::shared_ptr<Frame> frame) override {
        auto videoFrame = frame->as<VideoFrame>();
        videoFrame->setPixelType(outputDisparityFrame_.load(std::memory_order_relaxed) ? OB_PIXEL_DISPARITY : OB_PIXEL_DEPTH);
        VideoSensor::outputFrame(std::move(frame));
    }

private:
    std::atomic_bool outputDisparityFrame_{ false };
};

}  // namespace

EGODepthDevice::EGODepthDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
    init();

    // check and start heartbeat after initialization is complete
    checkAndStartHeartbeat();
}

EGODepthDevice::~EGODepthDevice() noexcept {}

void EGODepthDevice::init() {
    initSensorList();
    initProperties();

    // Depth work mode / preset support: Default <-> Factory Calib.
    // fetchDepthWorkModeList() is fault-tolerant: on firmware without the depth-alg-mode structures the
    // list stays empty and fixSensorList() keeps the legacy full sensor set unchanged.
    auto depthWorkModeManager = std::make_shared<EGODepthDepthWorkModeManager>(this);
    registerComponent(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER, depthWorkModeManager);
    auto presetManager = std::make_shared<EGODepthPresetManager>(this);
    registerComponent(OB_DEV_COMPONENT_PRESET_MANAGER, presetManager);

    fetchDeviceInfo();
    fetchExtensionInfo();

    deviceTimeFreq_ = 1000000;
    frameTimeFreq_  = 1000000;

    auto deviceClockSynchronizer = std::make_shared<DeviceClockSynchronizer>(this, deviceTimeFreq_, deviceTimeFreq_);
    registerComponent(OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER, deviceClockSynchronizer);
    registerComponent(OB_DEV_COMPONENT_FW_RECORD_PROFILE_MANAGER, [this]() { return std::make_shared<FwRecordProfileManager>(this); });

    auto hostTimeInfoManager = std::make_shared<HostTimeInfoManager>(this);
    registerComponent(OB_DEV_COMPONENT_HOST_TIME_INFO_MANAGER, hostTimeInfoManager);
    BEGIN_TRY_EXECUTE({ hostTimeInfoManager->setHostTimeInfoToDevice(); })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_WARN("Failed to initialize host time information for EGODepth device"); })

    // Apply the current depth work mode to the sensor tree (register/trim sensors and components).
    fixSensorList();
}

void EGODepthDevice::fetchDeviceInfo() {
    auto propServer = getPropertyServer();

    // Some EGODepth firmware has not implemented the version/control structures yet, so this read may throw.
    // Tolerate the failure (fall back to the info populated from enumeration) so device init and streaming
    // are not blocked.
    BEGIN_TRY_EXECUTE({
        auto version                      = propServer->getStructureDataT<OBVersionInfo>(OB_STRUCT_VERSION);
        deviceInfo_->name_                = version.deviceName;
        deviceInfo_->fwVersion_           = version.firmwareVersion;
        deviceInfo_->deviceSn_            = version.serialNumber;
        deviceInfo_->asicName_            = version.depthChip;
        deviceInfo_->hwVersion_           = version.hardwareVersion;
        deviceInfo_->type_                = static_cast<uint16_t>(version.deviceType);
        deviceInfo_->supportedSdkVersion_ = version.sdkVersion;
        deviceInfo_->subSystemVersion_    = version.subSystemVersion;
    })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_WARN("EGODepth: read OB_STRUCT_VERSION failed (firmware may not implement it); using enumeration info."); })

#ifdef __ANDROID__
    // Android needs to query usb connect type from the port; try candidates in priority order.
    {
        const OBSensorType kConnTypeCandidates[] = { OB_SENSOR_DEPTH,     OB_SENSOR_COLOR,   OB_SENSOR_COLOR_LEFT, OB_SENSOR_COLOR_RIGHT,
                                                     OB_SENSOR_COLOR_TOP, OB_SENSOR_IR_LEFT, OB_SENSOR_IR_RIGHT };
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

#ifdef __linux__
    // todo: net device does not have backend type
    const OBSensorType kBackendCandidates[] = { OB_SENSOR_DEPTH,     OB_SENSOR_COLOR,   OB_SENSOR_COLOR_LEFT, OB_SENSOR_COLOR_RIGHT,
                                                OB_SENSOR_COLOR_TOP, OB_SENSOR_IR_LEFT, OB_SENSOR_IR_RIGHT };
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

void EGODepthDevice::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {
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

void EGODepthDevice::initSensorList() {
    registerComponent(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY, [this]() {
        std::shared_ptr<FrameProcessorFactory> factory;
        TRY_EXECUTE({ factory = std::make_shared<FrameProcessorFactory>(this); })
        return factory;
    });

    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();

    // The four EGODepth UVC functions occupy ascending interface numbers; sort the enumerated UVC ports
    // by interface index and assign by position: Color group (IF0) / Depth (IF2) / IR group (IF4) /
    // single Color (IF6). Ordering (rather than a hardcoded infIndex value) tolerates descriptor shifts.
    std::vector<std::shared_ptr<const SourcePortInfo>> uvcPortInfos;
    for(const auto &portInfo: sourcePortInfoList) {
        if(portInfo->portType == SOURCE_PORT_USB_UVC) {
            uvcPortInfos.push_back(portInfo);
        }
    }
    std::sort(uvcPortInfos.begin(), uvcPortInfos.end(), [](const std::shared_ptr<const SourcePortInfo> &a, const std::shared_ptr<const SourcePortInfo> &b) {
        auto ua = std::dynamic_pointer_cast<const USBSourcePortInfo>(a);
        auto ub = std::dynamic_pointer_cast<const USBSourcePortInfo>(b);
        if(!ua || !ub) {
            return false;
        }
        return ua->infIndex < ub->infIndex;
    });

    std::shared_ptr<const SourcePortInfo> colorGroupPortInfo  = uvcPortInfos.size() > 0 ? uvcPortInfos[0] : nullptr;
    std::shared_ptr<const SourcePortInfo> depthPortInfo       = uvcPortInfos.size() > 1 ? uvcPortInfos[1] : nullptr;
    std::shared_ptr<const SourcePortInfo> irGroupPortInfo     = uvcPortInfos.size() > 2 ? uvcPortInfos[2] : nullptr;
    std::shared_ptr<const SourcePortInfo> singleColorPortInfo = uvcPortInfos.size() > 3 ? uvcPortInfos[3] : nullptr;

    // ---- Color group (shared UVC endpoint, cam_id demux: 0=Left, 1=Right, 2=Top) ----
    if(colorGroupPortInfo) {
        registerComponent(OB_DEV_COMPONENT_EGODEPTH_COLOR_DEMUXER, [this, colorGroupPortInfo]() {
            auto           port      = getSourcePort(colorGroupPortInfo);
            auto           videoPort = std::dynamic_pointer_cast<IVideoStreamPort>(port);
            EGODemuxConfig config;
            config.camIdToFrameType = { { static_cast<uint8_t>(0), OB_FRAME_COLOR_LEFT },
                                        { static_cast<uint8_t>(1), OB_FRAME_COLOR_RIGHT },
                                        { static_cast<uint8_t>(2), OB_FRAME_COLOR_TOP } };
            // The single vendor XU rides this same Color-group interface, so the stop command is sent on
            // the demuxer's own real port (vendorCmdPort left unset).
            //
            // STOP_COLOR_RIGHT (not STOP_COLOR) stops the 3-way multiplexed Color group: firmware maps this
            // command to the shared Color-group pipeline, keeping it distinct from the single depth-module
            // Color (which uses STOP_COLOR), so stopping one group never tears down the other.
            config.stopStreamPropertyId = OB_PROP_STOP_COLOR_RIGHT_STREAM_BOOL;
            return std::make_shared<EGOStreamDemuxer>(this, videoPort, config);
        });

        const std::vector<std::tuple<OBSensorType, DeviceComponentId, DeviceComponentId, uint8_t>> colorChannels = {
            std::make_tuple(OB_SENSOR_COLOR_LEFT, OB_DEV_COMPONENT_LEFT_COLOR_SENSOR, OB_DEV_COMPONENT_LEFT_COLOR_FRAME_PROCESSOR, static_cast<uint8_t>(0)),
            std::make_tuple(OB_SENSOR_COLOR_RIGHT, OB_DEV_COMPONENT_RIGHT_COLOR_SENSOR, OB_DEV_COMPONENT_RIGHT_COLOR_FRAME_PROCESSOR, static_cast<uint8_t>(1)),
            std::make_tuple(OB_SENSOR_COLOR_TOP, OB_DEV_COMPONENT_TOP_COLOR_SENSOR, OB_DEV_COMPONENT_TOP_COLOR_FRAME_PROCESSOR, static_cast<uint8_t>(2)),
        };

        for(const auto &colorChannel: colorChannels) {
            auto sensorType              = std::get<0>(colorChannel);
            auto sensorComponent         = std::get<1>(colorChannel);
            auto frameProcessorComponent = std::get<2>(colorChannel);
            auto camId                   = std::get<3>(colorChannel);

            registerComponent(
                sensorComponent,
                [this, sensorType, frameProcessorComponent, camId]() {
                    auto demuxer = getComponentT<EGOStreamDemuxer>(OB_DEV_COMPONENT_EGODEPTH_COLOR_DEMUXER);
                    auto port    = demuxer->createVirtualPort(camId);
                    auto sensor  = std::make_shared<EGOVideoSensor>(this, sensorType, port, OB_PROP_STOP_COLOR_STREAM_BOOL);

                    sensor->enableTimestampAnomalyDetection(false);

                    std::vector<FormatFilterConfig> formatFilterConfigs = {
                        { FormatFilterPolicy::REPLACE, OB_FORMAT_HEVC, OB_FORMAT_H265, nullptr },
                        { FormatFilterPolicy::REMOVE, OB_FORMAT_BGR, OB_FORMAT_ANY, nullptr },
                        { FormatFilterPolicy::REMOVE, OB_FORMAT_BGRA, OB_FORMAT_ANY, nullptr },
                    };
                    sensor->updateFormatFilterConfig(formatFilterConfigs);

                    auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorOverUvcSCR>(this, frameTimeFreq_);
                    sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

                    auto frameProcessor = getComponentT<FrameProcessor>(frameProcessorComponent, false);
                    if(frameProcessor) {
                        sensor->setFrameProcessor(frameProcessor.get());
                    }

                    initSensorStreamProfile(sensor);
                    return sensor;
                },
                true);
            registerSensorPortInfo(sensorType, colorGroupPortInfo);

            registerComponent(frameProcessorComponent, [this, sensorType]() {
                auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
                auto frameProcessor = factory->createFrameProcessor(sensorType);
                return frameProcessor;
            });
        }

        // The single vendor XU/property channel rides on the Color-group UVC interface.
        registerComponent(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR, [this, colorGroupPortInfo]() {
            auto port    = getSourcePort(colorGroupPortInfo);
            auto uvcPort = std::dynamic_pointer_cast<UvcDevicePort>(port);
            if(uvcPort) {
                uvcPort->updateXuUnit(OB_EGO_XU_UNIT);
            }
            auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
            return accessor;
        });

        registerComponent(OB_DEV_COMPONENT_DEVICE_MONITOR, [this, colorGroupPortInfo]() {
            auto port    = getSourcePort(colorGroupPortInfo);
            auto uvcPort = std::dynamic_pointer_cast<UvcDevicePort>(port);
            if(uvcPort) {
                uvcPort->updateXuUnit(OB_EGO_XU_UNIT);
            }
            auto devMonitor = std::make_shared<DeviceMonitor>(this, port);
            return devMonitor;
        });
    }

    // ---- IR group (shared UVC endpoint, cam_id demux: 0=Left, 1=Right; Y8) ----
    if(irGroupPortInfo) {
        registerComponent(OB_DEV_COMPONENT_EGODEPTH_IR_DEMUXER, [this, irGroupPortInfo, colorGroupPortInfo]() {
            auto           port      = getSourcePort(irGroupPortInfo);
            auto           videoPort = std::dynamic_pointer_cast<IVideoStreamPort>(port);
            EGODemuxConfig config;
            config.camIdToFrameType     = { { static_cast<uint8_t>(0), OB_FRAME_IR_LEFT }, { static_cast<uint8_t>(1), OB_FRAME_IR_RIGHT } };
            config.stopStreamPropertyId = OB_PROP_STOP_IR_STREAM_BOOL;
            // The single vendor XU lives on the Color-group interface, not on the IR endpoint; route the
            // IR stop command there so it reaches the XU.
            if(colorGroupPortInfo) {
                config.vendorCmdPort = getSourcePort(colorGroupPortInfo);
            }
            return std::make_shared<EGOStreamDemuxer>(this, videoPort, config);
        });

        const std::vector<std::tuple<OBSensorType, DeviceComponentId, DeviceComponentId, uint8_t>> irChannels = {
            std::make_tuple(OB_SENSOR_IR_LEFT, OB_DEV_COMPONENT_LEFT_IR_SENSOR, OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR, static_cast<uint8_t>(0)),
            std::make_tuple(OB_SENSOR_IR_RIGHT, OB_DEV_COMPONENT_RIGHT_IR_SENSOR, OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR, static_cast<uint8_t>(1)),
        };

        for(const auto &irChannel: irChannels) {
            auto sensorType              = std::get<0>(irChannel);
            auto sensorComponent         = std::get<1>(irChannel);
            auto frameProcessorComponent = std::get<2>(irChannel);
            auto camId                   = std::get<3>(irChannel);

            registerComponent(
                sensorComponent,
                [this, sensorType, frameProcessorComponent, camId]() {
                    auto demuxer = getComponentT<EGOStreamDemuxer>(OB_DEV_COMPONENT_EGODEPTH_IR_DEMUXER);
                    auto port    = demuxer->createVirtualPort(camId);
                    auto sensor  = std::make_shared<VideoSensor>(this, sensorType, port);

                    sensor->enableTimestampAnomalyDetection(false);

                    auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorOverUvcSCR>(this, frameTimeFreq_);
                    sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

                    auto frameProcessor = getComponentT<FrameProcessor>(frameProcessorComponent, false);
                    if(frameProcessor) {
                        sensor->setFrameProcessor(frameProcessor.get());
                    }

                    initSensorStreamProfile(sensor);
                    return sensor;
                },
                true);
            registerSensorPortInfo(sensorType, irGroupPortInfo);

            registerComponent(frameProcessorComponent, [this, sensorType]() {
                auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
                auto frameProcessor = factory->createFrameProcessor(sensorType);
                return frameProcessor;
            });
        }
    }

    // ---- Depth (exclusive UVC endpoint, 1:1 passthrough, output only; Y16) ----
    if(depthPortInfo) {
        registerComponent(
            OB_DEV_COMPONENT_DEPTH_SENSOR,
            [this, depthPortInfo]() {
                auto port   = getSourcePort(depthPortInfo);
                auto sensor = std::make_shared<EGODepthD2DSensor>(this, port);

                sensor->enableTimestampAnomalyDetection(false);

                auto videoFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorOverUvcSCR>(this, frameTimeFreq_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                // The initial property read establishes the frame semantic before streaming starts.
                bool hwD2D = true;
                BEGIN_TRY_EXECUTE({ hwD2D = getPropertyServer()->getPropertyValueT<bool>(OB_PROP_DISPARITY_TO_DEPTH_BOOL); })
                CATCH_EXCEPTION_AND_EXECUTE({})
                sensor->markOutputDisparityFrame(!hwD2D);
                if(frameProcessor) {
                    OBPropertyValue value{};
                    value.intValue = hwD2D;
                    frameProcessor->setPropertyValue(OB_PROP_DISPARITY_TO_DEPTH_BOOL, value);
                }

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_DEPTH, depthPortInfo);

        registerComponent(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_DEPTH);
            return frameProcessor;
        });
    }

    // ---- Single Color (exclusive UVC endpoint, depth-module Color; H264 1280x800) ----
    if(singleColorPortInfo) {
        registerComponent(
            OB_DEV_COMPONENT_COLOR_SENSOR,
            [this, singleColorPortInfo]() {
                auto port   = getSourcePort(singleColorPortInfo);
                auto sensor = std::make_shared<EGOVideoSensor>(this, OB_SENSOR_COLOR, port, OB_PROP_STOP_COLOR_STREAM_BOOL);

                sensor->enableTimestampAnomalyDetection(false);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REPLACE, OB_FORMAT_HEVC, OB_FORMAT_H265, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGR, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGRA, OB_FORMAT_ANY, nullptr },
                };
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
        registerSensorPortInfo(OB_SENSOR_COLOR, singleColorPortInfo);

        registerComponent(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_COLOR);
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
            // the gyro and accel are both on the same port; no correction filter applied.
            // EGODepth uses the DMC640 IMU, whose firmware outputs float physical quantities plus magnetometer.
            auto sharedHidPort  = getComponentT<SharedDataStreamPort>(OB_DEV_COMPONENT_SHARED_HID_STREAM);
            auto dataStreamPort = sharedHidPort->createClientPort();
            auto imuStreamer = std::make_shared<ImuStreamer>(this, dataStreamPort, std::vector<std::shared_ptr<IFilter>>{}, std::make_shared<DMC640Decoder>());
            return imuStreamer;
        });

        registerComponent(OB_DEV_COMPONENT_DIRECTION_KEY_SERVICE, [this]() {
            auto sharedHidPort = getComponentT<SharedDataStreamPort>(OB_DEV_COMPONENT_SHARED_HID_STREAM);
            return std::make_shared<DirectionKeyService>(this, sharedHidPort->createClientPort());
        });

        registerComponent(OB_DEV_COMPONENT_BATTERY_INFO_SERVICE, [this]() {
            auto sharedHidPort = getComponentT<SharedDataStreamPort>(OB_DEV_COMPONENT_SHARED_HID_STREAM);
            return std::make_shared<BatteryInfoService>(this, sharedHidPort->createClientPort());
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

        registerComponent(
            OB_DEV_COMPONENT_MAG_SENSOR,
            [this, imuPortInfo]() {
                auto port                 = getSourcePort(imuPortInfo);
                auto imuStreamer          = getComponentT<ImuStreamer>(OB_DEV_COMPONENT_IMU_STREAMER);
                auto imuStreamerSharedPtr = imuStreamer.get();
                auto sensor               = std::make_shared<MagSensor>(this, port, imuStreamerSharedPtr, true);

                sensor->enableTimestampAnomalyDetection(false);

                // Firmware IMU timestamp unit is microseconds (1MHz); pass through directly.
                auto imuFrameTimestampCalculator = std::make_shared<FrameTimestampCalculatorDirectly>(this, frameTimeFreq_);
                sensor->setFrameTimestampCalculator(imuFrameTimestampCalculator);

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_MAG, imuPortInfo);
    }
}

void EGODepthDevice::initProperties() {
    auto propertyServer = std::make_shared<PropertyServer>(this);

    auto sensors = getSensorTypeList();
    for(auto &sensor: sensors) {
        auto &sourcePortInfo = getSensorPortInfo(sensor);
        if(sensor == OB_SENSOR_COLOR_LEFT) {
            // Standard UVC PU/CT color controls sit on the Color-group interface (same as the vendor XU).
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

    // Device-level vendor properties ride on the single vendor XU (the Color-group UVC interface). The four
    // Color streams are controlled together through it; the firmware normalizes per-sensor.
    bool hasColorGroup = std::find(sensors.begin(), sensors.end(), OB_SENSOR_COLOR_LEFT) != sensors.end();
    if(hasColorGroup) {
        // Factory Calib removes the Depth sensor entry but leaves the IR controls available, so retain the
        // physical Depth UVC port for the shared Gain PU accessor.
        auto depthPortInfo = getSensorPortInfo(OB_SENSOR_DEPTH);
        auto depthUvcPropertyAccessor = std::make_shared<LazyPropertyAccessor>([this, depthPortInfo]() {
            auto port = getSourcePort(depthPortInfo);
            return std::make_shared<UvcPropertyAccessor>(port);
        });

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

        propertyServer->registerProperty(OB_PROP_COLOR_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
        // The two shared-endpoint groups stop themselves through their demuxers (sent once when the last
        // channel of the group closes): the Color group via OB_PROP_STOP_COLOR_RIGHT_STREAM_BOOL and the IR
        // group via OB_PROP_STOP_IR_STREAM_BOOL. Those are intentionally NOT registered here, so a single
        // channel closing never tears down the others still streaming on the shared endpoint.
        //
        // The two exclusive endpoints (Depth and the single depth-module Color) have no demuxer, so their
        // plain VideoSensor::stop() must send the stop command itself. Register those here so they route
        // through the single vendor XU (this Color-group accessor); without registration isPropertySupported()
        // is false and no stop command is ever sent when those streams stop. STOP_COLOR here targets only the
        // single Color (distinct from the group's STOP_COLOR_RIGHT), so it does not disturb the Color group.
        propertyServer->registerProperty(OB_PROP_STOP_DEPTH_STREAM_BOOL, "", "w", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_STOP_COLOR_STREAM_BOOL, "", "w", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_HOST_TIME_INFO, "", "w", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_DEVICE_SERIAL_NUMBER, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_DEVICE_TIME, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_GET_TF_CARD_CAPACITY, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_DEVICE_TEMPERATURE, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_REBOOT_DEVICE_BOOL, "w", "w", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_RECORDING_PROMPT_INT, "rw", "w", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_DEVICE_IN_RECOVERY_MODE_BOOL, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_BOOT_INTO_RECOVERY_MODE_BOOL, "w", "w", vendorPropertyAccessor);
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
        // Magnetometer counterpart of the accel/gyro properties above. MagSensor reads the current
        // odr / full-scale-range and the preset lists from these, and writes them back on stream start,
        // so they must be registered or the magnetometer stream fails with "Property not writable".
        propertyServer->registerProperty(OB_PROP_MAG_ODR_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_MAG_FULL_SCALE_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_GET_MAG_PRESETS_ODR_LIST, "", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_GET_MAG_PRESETS_FULL_SCALE_LIST, "", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_MAG_SWITCH_BOOL, "", "rw", vendorPropertyAccessor);
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
        // Transparent YAML passthrough interfaces for production calibration (SDK does not parse the data).
        propertyServer->registerProperty(OB_RAW_DATA_ALIGN_CALIB_YAML, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_RAW_DATA_IMU_CALIB_YAML, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_RAW_DATA_EXPORT_LOG, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_COLOR_BITRATE_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_COLOR_DYNAMIC_BITRATE_ENABLE_BOOL, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_COLOR_AE_MAX_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
        // Match G335LG: AE and exposure use the vendor XU, while gain uses the Depth UVC PU.
        propertyServer->registerProperty(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_DEPTH_AUTO_EXPOSURE_PRIORITY_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_DEPTH_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_DEPTH_GAIN_INT, "rw", "rw", depthUvcPropertyAccessor);
        propertyServer->aliasProperty(OB_PROP_IR_AUTO_EXPOSURE_BOOL, OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL);
        propertyServer->aliasProperty(OB_PROP_IR_EXPOSURE_INT, OB_PROP_DEPTH_EXPOSURE_INT);
        propertyServer->aliasProperty(OB_PROP_IR_GAIN_INT, OB_PROP_DEPTH_GAIN_INT);
        propertyServer->registerProperty(OB_PROP_IR_AE_MAX_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_IR_BRIGHTNESS_INT, "rw", "rw", vendorPropertyAccessor);
        // Depth work mode (preset) support: Default <-> Factory Calib
        propertyServer->registerProperty(OB_RAW_DATA_DEPTH_ALG_MODE_LIST, "r", "r", vendorPropertyAccessor);
        propertyServer->registerProperty(OB_STRUCT_CURRENT_DEPTH_ALG_MODE, "rw", "rw", vendorPropertyAccessor);
        // Effective video stream profile list for the current depth work mode.
        propertyServer->registerProperty(OB_RAW_DATA_EFFECTIVE_VIDEO_STREAM_PROFILE_LIST, "r", "r", vendorPropertyAccessor);
        // Keep the laser-control property path consistent with G335LG.
        propertyServer->registerProperty(OB_PROP_LASER_CONTROL_INT, "rw", "rw", vendorPropertyAccessor);
        // Reuse the existing hardware D2D switch shown by the Advanced Control panel.
        propertyServer->registerProperty(OB_PROP_DISPARITY_TO_DEPTH_BOOL, "rw", "rw", vendorPropertyAccessor);
        propertyServer->registerAccessCallback(
            OB_PROP_DISPARITY_TO_DEPTH_BOOL,
            [this](uint32_t, const uint8_t *data, size_t dataSize, PropertyOperationType operationType) {
                if(operationType != PROP_OP_WRITE || !data || dataSize < sizeof(uint32_t)) {
                    return;
                }

                auto sensor = getComponentT<EGODepthD2DSensor>(OB_DEV_COMPONENT_DEPTH_SENSOR, false);
                if(sensor) {
                    const bool hardwareD2DEnabled = *reinterpret_cast<const uint32_t *>(data) != 0;
                    sensor->markOutputDisparityFrame(!hardwareD2DEnabled);

                    auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);
                    if(frameProcessor) {
                        OBPropertyValue value{};
                        value.intValue = hardwareD2DEnabled;
                        frameProcessor->setPropertyValue(OB_PROP_DISPARITY_TO_DEPTH_BOOL, value);
                    }
                }
            });
        // Firmware-controlled hardware D2C switch; the firmware owns the output stream conversion.
        propertyServer->registerProperty(OB_PROP_DEPTH_ALIGN_HARDWARE_BOOL, "rw", "rw", vendorPropertyAccessor);

        auto heartbeatPropertyAccessor = std::make_shared<HeartbeatPropertyAccessor>(this);
        propertyServer->registerProperty(OB_PROP_HEARTBEAT_BOOL, "rw", "rw", heartbeatPropertyAccessor);
    }

    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, false);
    registerComponent(OB_DEV_COMPONENT_FW_LOG_MANAGER, [this]() { return std::make_shared<FirmwareLogManager>(this); });

    if(propertyServer->isPropertySupported(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        propertyServer->setPropertyValueT(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, OB_COMM_USB);
    }
}

std::vector<std::shared_ptr<IFilter>> EGODepthDevice::createRecommendedPostProcessingFilters(OBSensorType type) {
    // EGODepth streams have no recommended post-processing filters by default (Depth is output-only).
    utils::unusedVar(type);
    return {};
}

void EGODepthDevice::fixSensorList() {
    auto depthWorkModeManager = getComponentT<EGODepthDepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER, false);
    if(!depthWorkModeManager || depthWorkModeManager->getDepthWorkModeList().empty()) {
        // Firmware does not expose depth work modes: keep the legacy full sensor set unchanged.
        return;
    }

    const auto &currentMode = depthWorkModeManager->getCurrentDepthWorkMode();
    auto        propServer  = getPropertyServer();

    if(std::strcmp(currentMode.name, kEgoDepthFactoryCalibMode) == 0) {
        // Factory Calib: no Depth stream.
        deregisterSensor(OB_SENSOR_DEPTH);
        deregisterComponent(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        propServer->unregisterProperty(OB_PROP_STOP_DEPTH_STREAM_BOOL);
    }
    else {
        // Default: no left/right IR streams.
        deregisterSensor(OB_SENSOR_IR_LEFT);
        deregisterSensor(OB_SENSOR_IR_RIGHT);
        deregisterComponent(OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR);
        deregisterComponent(OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR);
        deregisterComponent(OB_DEV_COMPONENT_EGODEPTH_IR_DEMUXER);
        // Note: OB_PROP_STOP_IR_STREAM_BOOL is intentionally NOT registered in initProperties() (the IR
        // demuxer sends it when the last channel of the IR group closes), so nothing to unregister here.
    }

    std::vector<OBEffectiveStreamProfile> effectiveProfiles;
    bool                                  effectiveProfilesRead = false;
    BEGIN_TRY_EXECUTE({
        effectiveProfiles = (propServer->getStructureDataListProtoV1_1_T<OBEffectiveStreamProfile, 0>(OB_RAW_DATA_EFFECTIVE_VIDEO_STREAM_PROFILE_LIST));
        effectiveProfilesRead = true;
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
