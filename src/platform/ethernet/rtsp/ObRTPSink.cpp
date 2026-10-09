// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ObRTPSink.hpp"

#include "H264VideoRTPSource.hh"

#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"

#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <chrono>
#include <limits>

namespace libobsensor {

#define H264_NAL_SPS 7  // Sequence parameter set
#define H264_NAL_PPS 8  // Picture parameter set

#define H265_NAL_VPS 32  // Video Parameter Set
#define H265_NAL_SPS 33  // Sequence parameter set
#define H265_NAL_PPS 34  // Picture parameter set

#pragma pack(2)
typedef struct {
    uint64_t frameCounter;  // Frame number
    uint16_t extentionLen;  // Extended data length, default is 0
    uint64_t timestamp;     // timestamp
    uint32_t width;         // Frame pixel width
    uint32_t height;        // Frame pixel height
} OBNetworkFrameHeader;
struct MJPGMetadataHeader {
    uint8_t  magic[8];  // fixed magic:"ORBBEC"
    uint32_t data_len;  // Length of the entire metadata data structure

    bool validateMagic() const {
        const char expectedMagic[8] = "ORBBEC";
        return std::memcmp(magic, expectedMagic, sizeof(expectedMagic)) == 0;
    }
};
#pragma pack()

const std::vector<std::string> OBVendorCodecs = {
    "OB_FMT_Y16", "OB_FMT_Y8", "OB_FMT_Y10", "OB_FMT_RVL", "OB_FMT_MJPEG", "YUYV", "I420", "Y12C4", "OB_FMT_PCM"
};

bool isOBVendorCodec(const std::string &codec) {
    return std::find(OBVendorCodecs.begin(), OBVendorCodecs.end(), codec) != OBVendorCodecs.end();
}

const std::map<std::string, OBFormat> CodecToFormatMap = {
    { "H264", OB_FORMAT_H264 },      { "H265", OB_FORMAT_H265 },        { "JPEG", OB_FORMAT_MJPG },      { "OB_FMT_Y16", OB_FORMAT_Y16 },
    { "OB_FMT_Y8", OB_FORMAT_Y8 },   { "OB_FMT_Y10", OB_FORMAT_Y10 },   { "OB_FMT_RVL", OB_FORMAT_RVL }, { "OB_FMT_MJPEG", OB_FORMAT_MJPG },
    { "YUYV", OB_FORMAT_YUYV },      { "OB_FMT_YUYV", OB_FORMAT_YUYV }, { "I420", OB_FORMAT_I420 },      { "Y12C4", OB_FORMAT_Y12C4 },
    { "OB_FMT_PCM", OB_FORMAT_PCM },
};

OBFormat codecToOBFormat(const std::string &codec) {
    auto it = CodecToFormatMap.find(codec);
    if(it != CodecToFormatMap.end()) {
        return it->second;
    }
    return OB_FORMAT_UNKNOWN;
}

namespace {
constexpr uint32_t kLegacyVideoBufferBytes      = 3U * 3840U * 2160U;
constexpr uint32_t kPcmBufferBytes              = 65536U;
constexpr uint32_t kPcmBufferCount              = 32U;
constexpr uint32_t kLegacyVideoBufferCount      = 4U;
constexpr uint32_t kCodecStaticHeaderReserve    = 64U * 1024U;

bool hasCodecStaticHeader(MediaSubsession &subsession) {
    return std::strcmp(subsession.codecName(), "H264") == 0 || std::strcmp(subsession.codecName(), "H265") == 0;
}

const char *receiveModeToString(RTPReceiveMode mode) {
    switch(mode) {
    case RTPReceiveMode::Legacy:
        return "legacy";
    case RTPReceiveMode::ProfileDropOldest:
        return "profile-drop-oldest";
    default:
        return "unknown";
    }
}

const char *formatToString(OBFormat format) {
    switch(format) {
    case OB_FORMAT_YUYV:
        return "YUYV";
    case OB_FORMAT_MJPG:
        return "MJPG";
    case OB_FORMAT_H264:
        return "H264";
    case OB_FORMAT_H265:
        return "H265";
    case OB_FORMAT_HEVC:
        return "HEVC";
    case OB_FORMAT_Y16:
        return "Y16";
    case OB_FORMAT_Y8:
        return "Y8";
    case OB_FORMAT_Y10:
        return "Y10";
    case OB_FORMAT_I420:
        return "I420";
    case OB_FORMAT_RVL:
        return "RVL";
    case OB_FORMAT_PCM:
        return "PCM";
    default:
        return "UNKNOWN";
    }
}

void updateMaximum(std::atomic<uint64_t> &target, uint64_t value) {
    auto current = target.load(std::memory_order_relaxed);
    while(current < value && !target.compare_exchange_weak(current, value, std::memory_order_relaxed)) {}
}

uint32_t resolvePayloadBytes(const std::shared_ptr<const StreamProfile> &profile, MediaSubsession &,
                             const RTPReceiveBufferConfig &config) {
    if(profile->getFormat() == OB_FORMAT_PCM)
        return kPcmBufferBytes;
    if(config.mode == RTPReceiveMode::Legacy)
        return kLegacyVideoBufferBytes;
    if(config.maxPayloadBytes != 0)
        return config.maxPayloadBytes;
    if(!profile->is<VideoStreamProfile>())
        return 0;
    return profile->as<VideoStreamProfile>()->getMaxFrameDataSize();
}

uint32_t resolveBufferCount(const std::shared_ptr<const StreamProfile> &profile, MediaSubsession &subsession,
                            const RTPReceiveBufferConfig &config) {
    if(profile->getFormat() == OB_FORMAT_PCM)
        return kPcmBufferCount;
    if(config.mode == RTPReceiveMode::Legacy)
        return kLegacyVideoBufferCount;

    const uint64_t payload = resolvePayloadBytes(profile, subsession, config);
    const uint64_t allocation = payload + (hasCodecStaticHeader(subsession) ? kCodecStaticHeaderReserve : 0U);
    if(payload == 0 || allocation > (std::numeric_limits<uint32_t>::max)())
        return 0;
    const uint64_t byBudget = config.perSinkMemoryBudgetBytes == 0 ? config.preferredBufferCount
                                                                    : config.perSinkMemoryBudgetBytes / allocation;
    return static_cast<uint32_t>((std::min)(static_cast<uint64_t>(config.preferredBufferCount), byBudget));
}

uint32_t resolveAllocationBytes(const std::shared_ptr<const StreamProfile> &profile, MediaSubsession &subsession,
                                const RTPReceiveBufferConfig &config) {
    const uint64_t bytes = static_cast<uint64_t>(resolvePayloadBytes(profile, subsession, config))
                         + (config.mode == RTPReceiveMode::ProfileDropOldest && hasCodecStaticHeader(subsession) ? kCodecStaticHeaderReserve : 0U);
    return bytes > (std::numeric_limits<uint32_t>::max)() ? 0 : static_cast<uint32_t>(bytes);
}
}  // namespace

ObRTPSink *ObRTPSink::createNew(std::shared_ptr<const StreamProfile> streamProfile, UsageEnvironment &env, MediaSubsession &subsession,
                                MutableFrameCallback callback, char const *streamId, const RTPReceiveBufferConfig &rtpReceiveBufferConfig) {
    const auto bufferCount = resolveBufferCount(streamProfile, subsession, rtpReceiveBufferConfig);
    const auto allocationBytes = resolveAllocationBytes(streamProfile, subsession, rtpReceiveBufferConfig);
    if(bufferCount == 0 || allocationBytes == 0
       || (rtpReceiveBufferConfig.mode == RTPReceiveMode::ProfileDropOldest && streamProfile->getFormat() != OB_FORMAT_PCM
           && bufferCount < rtpReceiveBufferConfig.minBufferCount)) {
        env.setResultMsg("RTSP receive buffer configuration exceeds its memory budget");
        return nullptr;
    }
    return new ObRTPSink(streamProfile, env, subsession, callback, streamId, rtpReceiveBufferConfig);
}

ObRTPSink::ObRTPSink(std::shared_ptr<const StreamProfile> streamProfile, UsageEnvironment &env, MediaSubsession &subsession, MutableFrameCallback callback,
                     char const *streamId, const RTPReceiveBufferConfig &rtpReceiveBufferConfig)
    : MediaSink(env),
      subsession_(subsession),
      frameCallback_(callback),
      streamProfile_(streamProfile),
      rtpReceiveBufferConfig_(rtpReceiveBufferConfig),
      frameCount_(1),
      destroy_(false),
      currentBuffer_(nullptr),
      maxFrameNum_(resolveBufferCount(streamProfile, subsession, rtpReceiveBufferConfig)),
      maxFrameDataSize_(resolveAllocationBytes(streamProfile, subsession, rtpReceiveBufferConfig)) {
    streamId_ = strDup(streamId);
    envir() << "ObRTPSink created! streamId = " << streamId_ << "\n";

    auto     frame       = std::make_shared<ObRTPBuffer>(maxFrameDataSize_);
    auto     frameBuffer = frame->getBuffer();
    uint32_t headerSize  = 0;

    // Load vps/pps/sps into the frame header
    if(strcmp(subsession_.codecName(), "H264") == 0) {
        unsigned int  nalUnitNum;
        SPropRecord  *pSPropRecord;
        unsigned char nalUnitStartCode[4] = { 0, 0, 0, 1 };

        pSPropRecord = parseSPropParameterSets(subsession_.fmtp_spropparametersets(), nalUnitNum);
        for(unsigned int i = 0; i < nalUnitNum; i++) {
            memcpy(frameBuffer + headerSize, &nalUnitStartCode[0], 4);
            headerSize += 4;
            memcpy(frameBuffer + headerSize, pSPropRecord[i].sPropBytes, pSPropRecord[i].sPropLength);
            headerSize += pSPropRecord[i].sPropLength;
        }

        memcpy(frameBuffer + headerSize, &nalUnitStartCode[0], 4);
        headerSize += 4;
        frame->setHeaderSize(headerSize);
        delete[] pSPropRecord;
    }
    else if(strcmp(subsession_.codecName(), "H265") == 0) {
        SPropRecord *sPropRecords[3];
        unsigned     numSPropRecords[3];
        sPropRecords[0] = parseSPropParameterSets(subsession_.fmtp_spropvps(), numSPropRecords[0]);
        sPropRecords[1] = parseSPropParameterSets(subsession_.fmtp_spropsps(), numSPropRecords[1]);
        sPropRecords[2] = parseSPropParameterSets(subsession_.fmtp_sproppps(), numSPropRecords[2]);

        unsigned char nalUnitStartCode[4] = { 0, 0, 0, 1 };

        for(uint8_t i = 0; i < 3; i++) {
            for(uint32_t j = 0; j < numSPropRecords[i]; j++) {
                memcpy(frameBuffer + headerSize, &nalUnitStartCode[0], 4);
                headerSize += 4;
                memcpy(frameBuffer + headerSize, sPropRecords[i][j].sPropBytes, sPropRecords[i][j].sPropLength);
                headerSize += sPropRecords[i][j].sPropLength;
            }
            delete[] sPropRecords[i];
        }
        memcpy(frameBuffer + headerSize, &nalUnitStartCode[0], 4);
        headerSize += 4;
        frame->setHeaderSize(headerSize);
    }
    // else: header size is 0

    LOG_INFO("RTSP receive pool configured: stream={}, mode={}, format={}({}), buffers={}, payloadBytes={}, allocationBytes={}",
             streamId_ ? streamId_ : "", receiveModeToString(rtpReceiveBufferConfig_.mode), formatToString(streamProfile_->getFormat()),
             static_cast<int>(streamProfile_->getFormat()), maxFrameNum_,
             resolvePayloadBytes(streamProfile_, subsession, rtpReceiveBufferConfig_), maxFrameDataSize_);

    std::unique_lock<std::mutex> lk(reclaimedRTPBufferMutex_);
    for(uint32_t i = 0; i < maxFrameNum_ - 1; i++) {
        auto newFrame = frame->cloneAndCopyHeader();
        reclaimedRTPBufferQueue_.push(newFrame);
    }
    reclaimedRTPBufferQueue_.push(frame);

    outputFrameThread_ = std::thread(&ObRTPSink::outputFrameFunc, this);
}

ObRTPSink::~ObRTPSink() noexcept {
    envir() << "ObRTPSink destructor! streamId = " << streamId_ << "\n";
    destroy_       = true;
    frameAvailableCv_.notify_all();
    if(outputFrameThread_.joinable()) {
        outputFrameThread_.join();
    }
    frameCallback_ = nullptr;
    delete[] streamId_;
}

void ObRTPSink::afterGettingFrame(void *clientData, unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime,
                                  unsigned durationInMicroseconds) {
    ObRTPSink *sink = (ObRTPSink *)clientData;
    sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void ObRTPSink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime, unsigned /*duration in microseconds*/) {
    do {
        if(rtpReceiveBufferConfig_.mode == RTPReceiveMode::ProfileDropOldest && numTruncatedBytes != 0) {
            truncatedFrames_.fetch_add(1, std::memory_order_relaxed);
            truncatedBytes_.fetch_add(numTruncatedBytes, std::memory_order_relaxed);
            if(rtpReceiveBufferConfig_.waitForKeyFrameAfterDrop && isCompressedVideo()) {
                awaitingKeyFrame_.store(true, std::memory_order_relaxed);
            }
            break;
        }
        utils::unusedVar(numTruncatedBytes);
        if(rtpReceiveBufferConfig_.mode == RTPReceiveMode::ProfileDropOldest && frameSize == 0) {
            break;
        }
        if(strcmp(subsession_.codecName(), "H264") == 0) {
            auto recv = currentBuffer_->getRecvdDataBuffer();
            if((recv[0] & 0x1f) == H264_NAL_SPS || (recv[0] & 0x1f) == H264_NAL_PPS) {  // SPS(7) or PPS(8)
                break;
            }
        }
        else if(strcmp(subsession_.codecName(), "H265") == 0) {
            auto recv = currentBuffer_->getRecvdDataBuffer();
            if(((recv[0] & 0x7f) >> 1) == H265_NAL_VPS || ((recv[0] & 0x7f) >> 1) == H265_NAL_SPS || ((recv[0] & 0x7f) >> 1) == H265_NAL_PPS) {
                break;
            }
        }

        currentBuffer_->setRecvdDataSize(frameSize);
        currentBuffer_->setCodecName(subsession_.codecName());
        currentBuffer_->setSequenceNumber(frameCount_);
        auto tsp = subsession_.getNormalPlayTime(presentationTime);
        currentBuffer_->setTimestamp(static_cast<uint64_t>(tsp));
        currentBuffer_->setKeyFrame(isKeyFrame(*currentBuffer_));

        if(rtpReceiveBufferConfig_.mode == RTPReceiveMode::ProfileDropOldest && awaitingKeyFrame_.load(std::memory_order_relaxed)) {
            if(!currentBuffer_->isKeyFrame()) {
                droppedAwaitingKeyFrames_.fetch_add(1, std::memory_order_relaxed);
                break;
            }
            awaitingKeyFrame_.store(false, std::memory_order_relaxed);
        }
        currentBuffer_->setEnqueueSteadyTimeUs(utils::getSteadyTimeUs());

        {
            std::unique_lock<std::mutex> lk(outputRTPBufferQueueMutex_);
            outputRTPBufferQueue_.push(currentBuffer_);
            receivedFrames_.fetch_add(1, std::memory_order_relaxed);
            updateMaximum(outputQueuePeak_, static_cast<uint64_t>(outputRTPBufferQueue_.size()));
            currentBuffer_.reset();
        }
        frameAvailableCv_.notify_all();
        frameCount_++;
    } while(0);

    // Then continue, to request the next frame of data:
    continuePlaying();
}

Boolean ObRTPSink::continuePlaying() {
    if(fSource == NULL || frameCallback_ == NULL) {
        return False;
    }

    if(!currentBuffer_) {
        std::unique_lock<std::mutex> lk(reclaimedRTPBufferMutex_);
        if(!reclaimedRTPBufferQueue_.empty()) {
            currentBuffer_ = reclaimedRTPBufferQueue_.front();
            reclaimedRTPBufferQueue_.pop();
        }
    }

    if(!currentBuffer_) {
        // Actively drop frames
        std::unique_lock<std::mutex> lk(outputRTPBufferQueueMutex_);
        if(!outputRTPBufferQueue_.empty()) {
            currentBuffer_ = outputRTPBufferQueue_.front();
            outputRTPBufferQueue_.pop();
            if(rtpReceiveBufferConfig_.mode == RTPReceiveMode::ProfileDropOldest) {
                droppedPoolExhaustionFrames_.fetch_add(1, std::memory_order_relaxed);
                if(rtpReceiveBufferConfig_.waitForKeyFrameAfterDrop && isCompressedVideo()) {
                    awaitingKeyFrame_.store(true, std::memory_order_relaxed);
                }
            }
            if(rtpReceiveBufferConfig_.mode == RTPReceiveMode::Legacy) {
                ++legacyPoolExhaustionDropsSinceLastLog_;
                const auto now = std::chrono::steady_clock::now();
                if(lastLegacyPoolExhaustionLogTime_ == std::chrono::steady_clock::time_point{}
                   || now - lastLegacyPoolExhaustionLogTime_ >= std::chrono::seconds(5)) {
                    LOG_WARN("RTSP receive buffer exhausted; droppedOldestFrames={} in the last reporting interval: stream={}",
                             legacyPoolExhaustionDropsSinceLastLog_, streamId_ ? streamId_ : "");
                    legacyPoolExhaustionDropsSinceLastLog_ = 0;
                    lastLegacyPoolExhaustionLogTime_        = now;
                }
            }
        }
    }

    // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
    fSource->getNextFrame(currentBuffer_->getRecvdDataBuffer(), maxFrameDataSize_ - currentBuffer_->getStaticHeaderSize(), afterGettingFrame, this,
                          onSourceClosure, this);
    return true;
}

bool ObRTPSink::isCompressedVideo() const {
    return std::strcmp(subsession_.codecName(), "H264") == 0 || std::strcmp(subsession_.codecName(), "H265") == 0;
}

bool ObRTPSink::isKeyFrame(const ObRTPBuffer &buffer) const {
    if(!isCompressedVideo() || buffer.getRecvdDataSize() == 0) {
        return false;
    }
    const auto *data = buffer.getRecvdDataBuffer();
    if(std::strcmp(subsession_.codecName(), "H264") == 0) {
        return (data[0] & 0x1f) == 5;
    }
    const uint8_t nalType = static_cast<uint8_t>((data[0] >> 1) & 0x3f);
    return nalType >= 16 && nalType <= 23;
}

void ObRTPSink::outputFrameFunc() {
    auto lastStatsLog = std::chrono::steady_clock::now();
    while(!destroy_) {
        std::shared_ptr<ObRTPBuffer> output;
        std::vector<std::shared_ptr<ObRTPBuffer>> discarded;
        {
            std::unique_lock<std::mutex> lk(outputRTPBufferQueueMutex_);
            frameAvailableCv_.wait(lk, [&]() { return !outputRTPBufferQueue_.empty() || destroy_; });
            if(destroy_) {
                break;
            }

            if(rtpReceiveBufferConfig_.mode == RTPReceiveMode::ProfileDropOldest) {
                const uint64_t nowUs = utils::getSteadyTimeUs();
                const uint64_t maxDelayUs = static_cast<uint64_t>(rtpReceiveBufferConfig_.maxReadyQueueDelayMs) * 1000ULL;
                while(outputRTPBufferQueue_.size() > 1 && maxDelayUs != 0
                      && nowUs > outputRTPBufferQueue_.front()->getEnqueueSteadyTimeUs()
                      && nowUs - outputRTPBufferQueue_.front()->getEnqueueSteadyTimeUs() > maxDelayUs) {
                    discarded.emplace_back(outputRTPBufferQueue_.front());
                    outputRTPBufferQueue_.pop();
                    droppedBacklogFrames_.fetch_add(1, std::memory_order_relaxed);
                    if(rtpReceiveBufferConfig_.waitForKeyFrameAfterDrop && isCompressedVideo()) {
                        awaitingKeyFrame_.store(true, std::memory_order_relaxed);
                    }
                }
                while(!outputRTPBufferQueue_.empty() && awaitingKeyFrame_.load(std::memory_order_relaxed)
                      && !outputRTPBufferQueue_.front()->isKeyFrame()) {
                    discarded.emplace_back(outputRTPBufferQueue_.front());
                    outputRTPBufferQueue_.pop();
                    droppedAwaitingKeyFrames_.fetch_add(1, std::memory_order_relaxed);
                }
                if(!outputRTPBufferQueue_.empty() && awaitingKeyFrame_.load(std::memory_order_relaxed)
                   && outputRTPBufferQueue_.front()->isKeyFrame()) {
                    awaitingKeyFrame_.store(false, std::memory_order_relaxed);
                }
            }

            if(!outputRTPBufferQueue_.empty()) {
                output = outputRTPBufferQueue_.front();
                outputRTPBufferQueue_.pop();
                const auto enqueuedUs = output->getEnqueueSteadyTimeUs();
                if(enqueuedUs != 0) {
                    const auto nowUs = utils::getSteadyTimeUs();
                    if(nowUs >= enqueuedUs) {
                        updateMaximum(maxQueueDelayUs_, nowUs - enqueuedUs);
                    }
                }
            }
        }

        if(!discarded.empty()) {
            std::unique_lock<std::mutex> lk(reclaimedRTPBufferMutex_);
            for(auto &buffer: discarded) {
                reclaimedRTPBufferQueue_.push(buffer);
            }
        }

        if(!output) {
            continue;
        }

        auto codecName = output->getCodecName();
        auto format    = codecToOBFormat(codecName);

        do {
            const auto profileFormat = streamProfile_->getFormat();
            const bool isH265Alias   = (format == OB_FORMAT_H265 || format == OB_FORMAT_HEVC)
                                     && (profileFormat == OB_FORMAT_H265 || profileFormat == OB_FORMAT_HEVC);
            if(format != profileFormat && !isH265Alias) {
                LOG_ERROR("Received frame format is not same as required");
                break;
            }

            TRY_EXECUTE({
                uint32_t frameOffset = 0;
                auto     frame       = format == OB_FORMAT_PCM ? FrameFactory::createFrame(OB_FRAME_AUDIO, OB_FORMAT_PCM, output->getRecvdDataSize())
                                                               : FrameFactory::createFrameFromStreamProfile(streamProfile_);
                frame->setStreamProfile(streamProfile_);

                if(format == OB_FORMAT_PCM) {
                    frame->setTimeStampUsec(output->getTimestamp());
                    frame->setSystemTimeStampUsec(utils::getNowTimesUs());
                    frame->setSteadyTimeStampUsec(utils::getSteadyTimeUs());
                    frame->setNumber(output->getSequenceNumber());
                }
                else if(isOBVendorCodec(codecName)) {
                    frameOffset = sizeof(OBNetworkFrameHeader);
                    output->setDynamicHeaderSize(sizeof(OBNetworkFrameHeader));
                    auto header = output->getDynamicHeader<OBNetworkFrameHeader>();

                    frame->setTimeStampUsec(header->timestamp);
                    const auto timestamp = utils::getHostTimestampUs();
                    frame->setSystemTimeStampUsec(timestamp.systemTimeUs);
                    frame->setSteadyTimeStampUsec(timestamp.steadyTimeUs);
                    frame->setNumber(header->frameCounter);

                    if(header->extentionLen != 0) {
                        frame->updateMetadata(output->getRecvdDataBuffer() + frameOffset, header->extentionLen);
                        frameOffset += header->extentionLen;
                    }
                }
                else {
                    frame->setTimeStampUsec(output->getTimestamp());
                    const auto timestamp = utils::getHostTimestampUs();
                    frame->setSystemTimeStampUsec(timestamp.systemTimeUs);
                    frame->setSteadyTimeStampUsec(timestamp.steadyTimeUs);
                    frame->setNumber(output->getSequenceNumber());
                }

                if(output->getStaticHeaderSize() > 0) {
                    frame->updateData(output->getBuffer(), output->getRecvdDataSize() + output->getStaticHeaderSize());
                }
                else {
                    frame->updateData(output->getRecvdDataBuffer() + frameOffset, output->getRecvdDataSize() - frameOffset);
                }

                if(format == OB_FORMAT_MJPG) {
                    mjpgUpdateMetadata(frame);
                }

                frameCallback_(frame);
                deliveredFrames_.fetch_add(1, std::memory_order_relaxed);
            });
        } while(0);

        {
            std::unique_lock<std::mutex> lk(reclaimedRTPBufferMutex_);
            reclaimedRTPBufferQueue_.push(output);
        }

        if(rtpReceiveBufferConfig_.mode == RTPReceiveMode::ProfileDropOldest
           && std::chrono::steady_clock::now() - lastStatsLog >= std::chrono::seconds(5)) {
            LOG_INFO("RTSP buffer stats: stream={}, received={}, delivered={}, droppedPool={}, droppedBacklog={}, droppedAwaitKey={}, "
                     "truncatedFrames={}, truncatedBytes={}, queuePeak={}, maxQueueDelayUs={}",
                     streamId_ ? streamId_ : "", receivedFrames_.load(std::memory_order_relaxed), deliveredFrames_.load(std::memory_order_relaxed),
                     droppedPoolExhaustionFrames_.load(std::memory_order_relaxed), droppedBacklogFrames_.load(std::memory_order_relaxed),
                     droppedAwaitingKeyFrames_.load(std::memory_order_relaxed), truncatedFrames_.load(std::memory_order_relaxed),
                     truncatedBytes_.load(std::memory_order_relaxed), outputQueuePeak_.load(std::memory_order_relaxed),
                     maxQueueDelayUs_.load(std::memory_order_relaxed));
            lastStatsLog = std::chrono::steady_clock::now();
        }
    }
}

void ObRTPSink::mjpgUpdateMetadata(std::shared_ptr<Frame> frame) {
    if(!frame || !frame->getData()) {
        LOG_WARN("MJPG metadata update skipped: frame or frame data is null");
        return;
    }

    int            orgFrameSize = static_cast<int>(frame->getDataSize());
    const uint8_t *orgFrameData = frame->getData();

    int jpegHeadSize = utils::getJpgHeadLength(orgFrameData, orgFrameSize);
    if(jpegHeadSize == -1 || jpegHeadSize >= orgFrameSize) {
        return;
    }

    if(orgFrameData[jpegHeadSize] != 0xFF || orgFrameData[jpegHeadSize + 1] != 0xD8) {
        return;
    }

    int comSequenceIndex = utils::findJpgCOMSequence(frame->getData(), orgFrameSize, jpegHeadSize);
    if(comSequenceIndex == -1) {
        return;
    }

    const MJPGMetadataHeader *header = reinterpret_cast<const MJPGMetadataHeader *>(const_cast<uint8_t *>(frame->getData() + comSequenceIndex + 4));
    if(!header->validateMagic()) {
        return;
    }

    const int metadataSize = header->data_len;
    if(jpegHeadSize + metadataSize >= orgFrameSize) {
        LOG_WARN("MJPG metadata update skipped: metadata size exceeds frame boundary, metadataSize={}, jpegHeadSize={}, frameSize={}", metadataSize,
                 jpegHeadSize, orgFrameSize);
        return;
    }

    frame->updateMetadata(const_cast<uint8_t *>(frame->getData() + jpegHeadSize + 2 + 4), metadataSize);

    // Calculate COM segment end position
    // COM structure: 0xFF 0xFE [2-byte length (big-endian, includes itself)] [data...]
    int comLength      = (orgFrameData[comSequenceIndex + 2] << 8) | orgFrameData[comSequenceIndex + 3];
    int comEndPosition = comSequenceIndex + 2 + comLength;

    if(comEndPosition >= orgFrameSize) {
        LOG_WARN("MJPG metadata update skipped: COM end position exceeds frame size, comEndPosition={}, frameSize={}", comEndPosition, orgFrameSize);
        return;
    }

    // Search for SOS AFTER the COM segment to avoid false SOS detection in metadata content.
    // Metadata may contain 0xFF 0xDA bytes which would be incorrectly identified as SOS marker.
    int sosSearchOffset = comEndPosition - jpegHeadSize;
    int remainingSize   = orgFrameSize - comEndPosition;

    int sosIndex = utils::findJpgSOSSequence(orgFrameData + comEndPosition, remainingSize);
    if(sosIndex == -1) {
        LOG_WARN("MJPG metadata update skipped: SOS marker not found after COM segment, searchOffset={}, remainingSize={}", sosSearchOffset, remainingSize);
        return;
    }

    int secondJpegHeadSize = sosSearchOffset + sosIndex + 14;
    if(secondJpegHeadSize == -1 || secondJpegHeadSize >= orgFrameSize) {
        LOG_WARN("MJPG metadata update skipped: invalid second JPEG head size={}, frameSize={}", secondJpegHeadSize, orgFrameSize);
        return;
    }

    std::vector<uint8_t> data(orgFrameSize - secondJpegHeadSize);

    memcpy(data.data(), frame->getData(), jpegHeadSize);

    memcpy(data.data() + jpegHeadSize, frame->getData() + jpegHeadSize + secondJpegHeadSize, orgFrameSize - (jpegHeadSize + secondJpegHeadSize));

    frame->updateData(data.data(), data.size());
}

}  // namespace libobsensor
