// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "ISourcePort.hpp"
#include "ethernet/socket/VendorTCPClient.hpp"
#include <set>
#include <thread>
#include <mutex>

namespace libobsensor {

// Default TCP receive packet size (bytes) for a vendor data stream. Devices whose stream packs more data
// per packet (e.g. EGODepth's DMC640 IMU: 8B HID header + 10 x 64B groups = 648B) pass a larger value.
static const uint32_t OB_NET_DATA_STREAM_DEFAULT_PACKET_SIZE = 248;

struct NetDataStreamPortInfo : public NetSourcePortInfo {
    NetDataStreamPortInfo(const NetSourcePortInfo &portInfo, uint16_t newPort, uint16_t vendorPort,
                          uint32_t packetSize = OB_NET_DATA_STREAM_DEFAULT_PACKET_SIZE)
        : NetSourcePortInfo(portInfo), vendorPort(vendorPort), packetSize(packetSize) {
        port     = newPort;
        portType = SOURCE_PORT_NET_VENDOR_STREAM;
    }

    virtual bool equal(std::shared_ptr<const SourcePortInfo> cmpInfo) const override {
        if(cmpInfo->portType != portType) {
            return false;
        }
        auto netCmpInfo = std::dynamic_pointer_cast<const NetDataStreamPortInfo>(cmpInfo);
        return (address == netCmpInfo->address) && (port == netCmpInfo->port) && (vendorPort == netCmpInfo->vendorPort);
    };

    uint16_t vendorPort;
    uint32_t packetSize;
};

class NetDataStreamPort : public IDataStreamPort {
public:
    NetDataStreamPort(std::shared_ptr<const NetDataStreamPortInfo> portInfo);
    virtual ~NetDataStreamPort() noexcept override;
    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override {
        return portInfo_;
    }

    void startStream(MutableFrameCallback callback) override;
    void stopStream() override;

public:
    void readData();

private:
    std::shared_ptr<const NetDataStreamPortInfo> portInfo_;
    bool                                         isStreaming_;

    std::shared_ptr<VendorTCPClient> tcpClient_;
    std::thread                      readDataThread_;

    MutableFrameCallback callback_;
};

}  // namespace libobsensor
