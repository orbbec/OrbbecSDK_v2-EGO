// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#include "LinuxScsiDevicePort.hpp"
#include "usb/enumerator/UsbEnumeratorLibusb.hpp"
#include "exception/ObException.hpp"
#include <libusb.h>
#include <array>
#include <cstring>

namespace libobsensor {
namespace {
void putLe32(uint8_t *dst, uint32_t value) {
    dst[0] = static_cast<uint8_t>(value);
    dst[1] = static_cast<uint8_t>(value >> 8);
    dst[2] = static_cast<uint8_t>(value >> 16);
    dst[3] = static_cast<uint8_t>(value >> 24);
}
uint32_t getLe32(const uint8_t *src) {
    return static_cast<uint32_t>(src[0]) | (static_cast<uint32_t>(src[1]) << 8) | (static_cast<uint32_t>(src[2]) << 16) | (static_cast<uint32_t>(src[3]) << 24);
}
}  // namespace

LinuxScsiDevicePort::LinuxScsiDevicePort(const std::shared_ptr<IUsbDevice> &usbDevice, std::shared_ptr<const USBSourcePortInfo> portInfo)
    : usbDevice_(usbDevice), portInfo_(std::move(portInfo)), interface_(portInfo_->infIndex) {
    auto device = std::dynamic_pointer_cast<UsbDeviceLibusb>(usbDevice_);
    if(!device) {
        THROW_IO_EXCEPTION("EGO mass-storage device is not backed by libusb");
    }
    auto handle  = device->getLibusbDeviceHandle();
    constexpr auto kBulkTransferType = static_cast<libusb_endpoint_transfer_type>(LIBUSB_TRANSFER_TYPE_BULK);
    endpointIn_  = device->getEndpointDesc(interface_, kBulkTransferType, LIBUSB_ENDPOINT_IN).bEndpointAddress;
    endpointOut_ = device->getEndpointDesc(interface_, kBulkTransferType, LIBUSB_ENDPOINT_OUT).bEndpointAddress;
    if(endpointIn_ == 0 || endpointOut_ == 0) {
        THROW_IO_EXCEPTION("EGO mass-storage bulk endpoints were not found");
    }
    if(libusb_kernel_driver_active(handle, interface_) == 1) {
        if(libusb_detach_kernel_driver(handle, interface_) != LIBUSB_SUCCESS) {
            THROW_IO_EXCEPTION("Failed to detach mass-storage kernel driver");
        }
        detachedKernelDriver_ = true;
    }
    if(libusb_claim_interface(handle, interface_) != LIBUSB_SUCCESS) {
        if(detachedKernelDriver_) {
            libusb_attach_kernel_driver(handle, interface_);
        }
        THROW_IO_EXCEPTION("Failed to claim EGO mass-storage interface");
    }
}

LinuxScsiDevicePort::~LinuxScsiDevicePort() noexcept {
    auto device = std::dynamic_pointer_cast<UsbDeviceLibusb>(usbDevice_);
    if(!device) {
        return;
    }
    auto handle = device->getLibusbDeviceHandle();
    libusb_release_interface(handle, interface_);
    if(detachedKernelDriver_) {
        libusb_attach_kernel_driver(handle, interface_);
    }
}

void LinuxScsiDevicePort::execute(const uint8_t *cdb, uint8_t cdbSize, ScsiDataDirection direction, uint8_t *data, uint32_t dataSize, uint32_t timeoutMs) {
    if(!cdb || cdbSize == 0 || cdbSize > 16 || (dataSize && !data)) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid SCSI request");
    }
    auto                    device = std::dynamic_pointer_cast<UsbDeviceLibusb>(usbDevice_);
    auto                    handle = device->getLibusbDeviceHandle();
    std::array<uint8_t, 31> cbw{};
    std::memcpy(cbw.data(), "USBC", 4);
    const auto currentTag = tag_++;
    putLe32(cbw.data() + 4, currentTag);
    putLe32(cbw.data() + 8, dataSize);
    cbw[12] = 0;
    if(direction == ScsiDataDirection::In) {
        cbw[12] = 0x80;
    }
    cbw[14] = cdbSize;
    std::memcpy(cbw.data() + 15, cdb, cdbSize);
    int  actual = 0;
    auto rc     = libusb_bulk_transfer(handle, endpointOut_, cbw.data(), static_cast<int>(cbw.size()), &actual, timeoutMs);
    if(rc != LIBUSB_SUCCESS || actual != static_cast<int>(cbw.size())) {
        THROW_IO_EXCEPTION("Failed to send SCSI command wrapper");
    }
    if(dataSize) {
        uint8_t endpoint = endpointOut_;
        if(direction == ScsiDataDirection::In) {
            endpoint = endpointIn_;
        }
        rc = libusb_bulk_transfer(handle, endpoint, data, static_cast<int>(dataSize), &actual, timeoutMs);
        if(rc != LIBUSB_SUCCESS || actual != static_cast<int>(dataSize)) {
            THROW_IO_EXCEPTION("SCSI data transfer failed");
        }
    }
    std::array<uint8_t, 13> csw{};
    rc = libusb_bulk_transfer(handle, endpointIn_, csw.data(), static_cast<int>(csw.size()), &actual, timeoutMs);
    const bool transferFailed  = rc != LIBUSB_SUCCESS;
    const bool sizeInvalid     = actual != static_cast<int>(csw.size());
    const bool signatureInvalid = getLe32(csw.data()) != 0x53425355;
    const bool tagInvalid       = getLe32(csw.data() + 4) != currentTag;
    const bool commandFailed    = csw[12] != 0;
    if(transferFailed || sizeInvalid || signatureInvalid || tagInvalid || commandFailed) {
        THROW_IO_EXCEPTION("Invalid or failed SCSI command status wrapper");
    }
}

std::shared_ptr<const SourcePortInfo> LinuxScsiDevicePort::getSourcePortInfo() const {
    return portInfo_;
}
}  // namespace libobsensor
