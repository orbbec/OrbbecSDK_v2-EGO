// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "VendorTCPClient.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
#include <windows.h>
#include <iphlpapi.h>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#endif

#include <thread>

#ifndef SHUT_WR
#ifdef _WIN32
#define SHUT_WR SD_SEND
#define SHUT_RD SD_RECEIVE
#define SHUT_RDWR SD_BOTH
#else
#error "SHUT_WR is not defined and this is not a Windows platform"
#endif
#endif

namespace libobsensor {

VendorTCPClient::VendorTCPClient(std::string localAddress, std::string localMac, std::string address, uint16_t port, uint32_t connectTimeout,
                                 uint32_t commTimeout)
    : isValidIP_(false),
      localAddress_(localAddress),
      localMac_(localMac),
      address_(address),
      port_(port),
      socketFd_(INVALID_SOCKET),
      flushed_(false),
      CONNECT_TIMEOUT_MS(connectTimeout),
      COMM_TIMEOUT_MS(commTimeout) {
    LOG_DEBUG("VendorTCPClient create localAddress:{}, localMac:{}", localAddress_, localMac_);
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    WSADATA wsaData;
    int     rst = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if(rst != 0) {
        THROW_IO_EXCEPTION(utils::string::to_string() << "Failed to load Winsock! err_code=" << GET_LAST_ERROR());
    }
#endif
    checkLocalIP();
// Due to network configuration changes causing socket connection pipeline errors, macOS throws a SIGPIPE exception to the application, leading to a crash (this
// does not occur on Linux). This exception needs to be filtered out.
#if(defined(OS_IOS) || defined(OS_MACOS))
    signal(SIGPIPE, SIG_IGN);
#endif
    socketConnect();
    LOG_DEBUG("VendorTCPClient Constructor ");
}

void VendorTCPClient::checkLocalIP() {
    isValidIP_ = false;
    unsigned int a, b, c, d;
    if(sscanf(localAddress_.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
        if((a == 10) ||                         // 10.0.0.0 - 10.255.255.255
           (a == 172 && b >= 16 && b <= 31) ||  // 172.16.0.0 - 172.31.255.255
           (a == 192 && b == 168) ||            // 192.168.0.0 - 192.168.255.255
           (a == 169 && b == 254)) {            // 169.254.0.0 - 169.254.255.255
            isValidIP_ = true;
        }
    }

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    if(!isValidIP_) {
        localAddress_ = "";
        IP_ADAPTER_INFO adapterInfo[16];  // Buffer to hold adapter info
        DWORD           bufLen = sizeof(adapterInfo);

        // Get the network adapter information
        DWORD dwRetVal = GetAdaptersInfo(adapterInfo, &bufLen);
        if(dwRetVal == ERROR_SUCCESS) {
            // Loop through all the adapters
            PIP_ADAPTER_INFO pAdapterInfo = adapterInfo;
            while(pAdapterInfo) {
                // Convert MAC address to string format
                std::ostringstream oss;
                for(int i = 0; i < 6; ++i) {
                    if(i > 0)
                        oss << ":";
                    oss << std::setw(2) << std::setfill('0') << std::hex << (int)pAdapterInfo->Address[i];
                }

                // Compare MAC address with the input MAC address
                std::string currentMac = oss.str();
                if(currentMac == localMac_) {
                    // If MAC matches, get the associated IP address (return the first one)
                    PIP_ADDR_STRING pIpAddr = &pAdapterInfo->IpAddressList;
                    if(pIpAddr) {
                        localAddress_ = pIpAddr->IpAddress.String;  // Return the first IP address
                    }
                    break;  // If no IP address found, break out of the loop
                }
                pAdapterInfo = pAdapterInfo->Next;
            }
        }

        if(sscanf(localAddress_.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            if((a == 10) ||                         // 10.0.0.0 - 10.255.255.255
               (a == 172 && b >= 16 && b <= 31) ||  // 172.16.0.0 - 172.31.255.255
               (a == 192 && b == 168) ||            // 192.168.0.0 - 192.168.255.255
               (a == 169 && b == 254)) {            // 169.254.0.0 - 169.254.255.255
                isValidIP_ = true;
            }
        }
    }
#endif
}

VendorTCPClient::~VendorTCPClient() noexcept {
    socketClose();
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    WSACleanup();
#endif

    LOG_DEBUG("VendorTCPClient Destructor ");
}

bool VendorTCPClient::checkConnectReady(SOCKET socket, uint32_t timeoutMs) {
    // Lambda: call select once, check writability and connection error via getsockopt.
    // fd_set must be re-initialized before every select call because select modifies them.
    auto checkReady = [&](SOCKET sock) -> bool {
        fd_set write, err;
        FD_ZERO(&write);
        FD_ZERO(&err);
        FD_SET(sock, &write);
        FD_SET(sock, &err);

        TIMEVAL connTimeout;
#if(defined(OS_IOS) || defined(OS_MACOS) || defined(__ANDROID__))
        connTimeout.tv_sec  = 0;
        connTimeout.tv_usec = 100000;  // 100ms
#else
        connTimeout.tv_sec  = timeoutMs / 1000;
        connTimeout.tv_usec = timeoutMs % 1000 * 1000;
#endif

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
        auto rst = select(0, NULL, &write, &err, &connTimeout);  // nfds ignored on Windows
#else
        auto rst            = select(sock + 1, NULL, &write, &err, &connTimeout);
#endif
        if(rst <= 0) {
            return false;  // timeout or error
        }
        if(!FD_ISSET(sock, &write)) {
            return false;  // connection failed
        }
        // Verify the connection actually succeeded (works on all platforms)
        int       sockErr = 0;
        socklen_t errLen  = sizeof(sockErr);
        if(getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&sockErr), &errLen) < 0 || sockErr != 0) {
            return false;
        }
        return true;
    };

#if(defined(OS_IOS) || defined(OS_MACOS) || defined(__ANDROID__))
    // Since the traditional `fcntl` cannot be used on iOS to set socketFd as non-blocking, leading to select blocking, reduce connTimeout and use polling to
    // avoid the issue.
    (void)timeoutMs;
    bool status = false;
    int  retry  = 5;
    do {
        status = checkReady(socket);
    } while(!status && retry-- > 0);

    return status;
#else
    return checkReady(socket);
#endif
}

void VendorTCPClient::socketConnect() {
    int rst;
    socketFd_ = socket(AF_INET, SOCK_STREAM, 0);  // ipv4, tcp(Streaming)
    if(socketFd_ == INVALID_SOCKET) {
        THROW_IO_EXCEPTION(utils::string::to_string() << "create socket failed! err_code=" << GET_LAST_ERROR());
    }

    LOG_DEBUG("TCP client socket: create socket={}", socketFd_);
    applyCommTimeout();

    // Add SO_LINGER configuration here
    struct linger linger_opt;
    linger_opt.l_onoff  = 1;  // Enable SO_LINGER
    linger_opt.l_linger = 0;  // Set linger timeout to 0 seconds
    if(setsockopt(socketFd_, SOL_SOCKET, SO_LINGER, (char *)&linger_opt, sizeof(linger_opt)) < 0) {
        LOG_WARN("Failed to set SO_LINGER option");
    }

    // Adjust the window size to resolve the network retransmission issue at any frame rate @LingYi
    // Adjust the window size
    // int sendBufSize = 1024 * 1024 * 2;  // 2MB
    // rst = setsockopt(socketFd_, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufSize, sizeof(sendBufSize));
    // Receive window size
    // int recvBufSize = 1024 * 1024 * 2;  // 2MB
    // rst = setsockopt(socketFd_, SOL_SOCKET, SO_RCVBUF, (char*)&recvBufSize, sizeof(recvBufSize));

    unsigned long mode = 1;  // non-blocking mode
    // Set to non-blocking; handle connect timeout
    rst = ioctlsocket(socketFd_, FIONBIO, &mode);
    if(rst < 0) {
        rst = GET_LAST_ERROR();
        socketClose();
        THROW_INVALID_PARAM_EXCEPTION(utils::string::to_string() << "VendorTCPClient: ioctlsocket to non-blocking mode failed! addr=" << address_
                                                                 << ", port=" << port_ << ", err_code=" << rst);
    }

    if(isValidIP_) {
        LOG_DEBUG("bind local address {}", localAddress_);
        struct sockaddr_in localAddr;
        localAddr.sin_family = AF_INET;
        localAddr.sin_port   = htons(0);
        if(inet_pton(AF_INET, localAddress_.c_str(), &localAddr.sin_addr) <= 0) {
            rst = GET_LAST_ERROR();
            socketClose();
            THROW_INVALID_PARAM_EXCEPTION(utils::string::to_string()
                                          << "VendorTCPClient: Invalid local ip address! addr=" << localAddress_ << ", err_code=" << rst);
        }

        if(bind(socketFd_, (struct sockaddr *)&localAddr, sizeof(localAddr)) < 0) {
            rst = GET_LAST_ERROR();
            socketClose();
            THROW_INVALID_PARAM_EXCEPTION(utils::string::to_string()
                                          << "VendorTCPClient: local ip bind failed! addr=" << localAddress_ << ", err_code=" << rst);
        }
        LOG_DEBUG("bind local address success!");
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;                                       // ipv4
    serverAddr.sin_port   = htons(port_);                                  // convert uint16_t from host to network byte sequence
    if(inet_pton(AF_INET, address_.c_str(), &serverAddr.sin_addr) <= 0) {  // address string to sin_addr
        rst = GET_LAST_ERROR();
        socketClose();
        THROW_INVALID_PARAM_EXCEPTION("Invalid address! err_code=" + std::to_string(rst));
    }

    rst = connect(socketFd_, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if(rst < 0) {
        rst = GET_LAST_ERROR();
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
        if(rst != WSAEWOULDBLOCK) {
#else
        if(rst != EINPROGRESS) {  // EINPROGRESS due to non-blocking mode
#endif
            socketClose();
            THROW_INVALID_PARAM_EXCEPTION(utils::string::to_string()
                                          << "VendorTCPClient: Connect to server failed! addr=" << address_ << ", port=" << port_ << ", err_code=" << rst);
        }
    }

    // Check if connect is ready
    if(!checkConnectReady(socketFd_, CONNECT_TIMEOUT_MS)) {
        socketClose();
        THROW_INVALID_PARAM_EXCEPTION(utils::string::to_string() << "VendorTCPClient: Connect to server failed! addr=" << address_ << ", port=" << port_
                                                                 << ", err=socket is not ready & timeout");
    }

    // Restore to blocking mode
    mode = 0;  // blocking mode
    rst  = ioctlsocket(socketFd_, FIONBIO, &mode);
    if(rst < 0) {
        socketClose();
        THROW_INVALID_PARAM_EXCEPTION(utils::string::to_string() << "VendorTCPClient: ioctlsocket to blocking mode failed! addr=" << address_
                                                                 << ", port=" << port_ << ", err_code=" << GET_LAST_ERROR());
    }
    LOG_DEBUG("TCP client socket created!, addr={0}, port={1}, socket={2}", address_, port_, socketFd_);
}

void VendorTCPClient::setTimeouts(uint32_t connectTimeoutMs, uint32_t commTimeoutMs) {
    if(connectTimeoutMs == 0 || commTimeoutMs == 0) {
        THROW_INVALID_PARAM_EXCEPTION("VendorTCPClient timeout must be greater than 0");
    }

    std::lock_guard<std::mutex> lock(tcpMtx_);
    CONNECT_TIMEOUT_MS          = connectTimeoutMs;
    COMM_TIMEOUT_MS             = commTimeoutMs;
    isCommTimeoutConfigured_    = true;

    applyCommTimeout();
    LOG_DEBUG("Vendor TCP timeout updated: connect={}ms, comm={}ms, addr={}, port={}", CONNECT_TIMEOUT_MS, COMM_TIMEOUT_MS, address_, port_);
}

void VendorTCPClient::applyCommTimeout() {
    if(socketFd_ == INVALID_SOCKET) {
        return;
    }

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    const uint32_t effectiveCommTimeoutMs = COMM_TIMEOUT_MS;
    uint32_t       commTimeout            = effectiveCommTimeoutMs;
#else
    const uint32_t effectiveCommTimeoutMs = isCommTimeoutConfigured_ ? COMM_TIMEOUT_MS : 2000;
    TIMEVAL        commTimeout{};
    commTimeout.tv_sec  = effectiveCommTimeoutMs / 1000;
    commTimeout.tv_usec = (effectiveCommTimeoutMs % 1000) * 1000;
#endif

    if(setsockopt(socketFd_, SOL_SOCKET, SO_SNDTIMEO, (char *)&commTimeout, sizeof(commTimeout)) < 0) {
        LOG_WARN("Set SO_SNDTIMEO failed: timeout={}ms, err={}", effectiveCommTimeoutMs, GET_LAST_ERROR());
    }

    if(setsockopt(socketFd_, SOL_SOCKET, SO_RCVTIMEO, (char *)&commTimeout, sizeof(commTimeout)) < 0) {
        LOG_WARN("Set SO_RCVTIMEO failed: timeout={}ms, err={}", effectiveCommTimeoutMs, GET_LAST_ERROR());
    }

}

void VendorTCPClient::socketClose() {
    // Guard against closing an already-invalidated socket. Use the portable "!= INVALID_SOCKET" check
    // (instead of "> 0") so the sentinel stays consistent with socketConnect()/flush() on all platforms
    // (Windows: SOCKET is an unsigned UINT_PTR and INVALID_SOCKET is ~0; POSIX: SOCKET is int and -1).
    if(socketFd_ != INVALID_SOCKET) {
        // Capture the error code immediately after the call: intervening calls (e.g. logging) may
        // overwrite the thread-local last-error, so deferring GET_LAST_ERROR() could report a stale code.
        auto rst     = closesocket(socketFd_);
        auto lastErr = GET_LAST_ERROR();
        if(rst < 0) {
            LOG_ERROR("close socket failed! socket={0}, err_code={1}", socketFd_, lastErr);
        }
        LOG_DEBUG("TCP client socket closed! socket={}", socketFd_);
        socketFd_ = INVALID_SOCKET;
    }
}

void VendorTCPClient::socketReconnect() {
    LOG_INFO("TCP client socket reconnecting... {}", address_);
    socketClose();
    socketConnect();
}

int VendorTCPClient::read(uint8_t *data, const uint32_t dataLen, utils::TimeInterval *interval) {
    std::lock_guard<std::mutex> lock(tcpMtx_);
    {
        uint8_t retry = 2;
        while(retry-- && !flushed_) {
            utils::TimingScope guard(interval);
            int                rst = 0;
#if defined(__linux__)
            rst = recv(socketFd_, (char *)data, dataLen, MSG_NOSIGNAL);
#else
            rst = recv(socketFd_, (char *)data, dataLen, 0);
#endif

            if(rst == 0) {
                // Peer has performed an orderly shutdown (recv returning 0 means the connection is closed).
                if(retry >= 1) {
                    LOG_INFO("reconnect...");
                    socketReconnect();
                    continue;
                }
                return -1;
            }
            else if(rst < 0) {
                rst = GET_LAST_ERROR();
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
                if(rst == WSAETIMEDOUT) {
                    // Timed out with no data; return 0 to indicate "no data available" (normal case).
                    return 0;
                }
                else if((rst == WSAECONNRESET || rst == WSAENOTCONN) && retry >= 1) {
                    LOG_INFO("RESET or NOTCONN ,reconnect...");
                    socketReconnect();
                    return -1;
                }
#else
                if(rst == EAGAIN || rst == EWOULDBLOCK) {
                    // Timed out with no data; return 0 to indicate "no data available" (normal case).
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    return 0;
                }
                else if(rst == EINTR) {
                    // Interrupted by a signal; retry directly.
                    continue;
                }
                else if((rst == ENOTCONN || rst == ECONNRESET || rst == ETIMEDOUT) && retry >= 1) {
                    socketReconnect();
                    return -1;
                }
#endif
                else {
                    THROW_IO_EXCEPTION(utils::string::to_string() << "VendorTCPClient read data failed! socket=" << socketFd_ << ", err_code=" << rst);
                }
            }
            else {
                return rst;
            }
        }

        return -1;
    }
}

void VendorTCPClient::write(const uint8_t *data, const uint32_t dataLen, utils::TimeInterval *interval) {
    std::lock_guard<std::mutex> lock(tcpMtx_);
    {
        uint8_t retry = 2;
        while(retry-- && !flushed_) {
            utils::TimingScope guard(interval);
            int                rst = 0;
#if defined(__linux__)
            rst = send(socketFd_, (const char *)data, dataLen, MSG_NOSIGNAL);
#else
            rst = send(socketFd_, (const char *)data, dataLen, 0);
#endif

            if(rst < 0) {
                rst = GET_LAST_ERROR();
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
                if((rst == WSAECONNRESET || rst == WSAENOTCONN || rst == WSAETIMEDOUT) && retry >= 1) {
#else
                if((rst == EAGAIN || rst == EWOULDBLOCK) && retry >= 1) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                else if((rst == ENOTCONN || rst == ECONNRESET || rst == ETIMEDOUT) && retry >= 1) {
#endif
                    socketReconnect();
                }
                else {
                    THROW_IO_EXCEPTION(utils::string::to_string() << "VendorTCPClient write data failed! socket=" << socketFd_ << ", err_code=" << rst);
                }
                continue;
            }
            break;
        }
    }
}

void VendorTCPClient::flush() {
    std::lock_guard<std::mutex> lock(tcpMtx_);
    {
        if(socketFd_ != INVALID_SOCKET) {
            LOG_DEBUG("TCP client socket flush.");
            flushed_ = true;
            shutdown(socketFd_, SHUT_WR);
            char buf[1024];
            utils::Timer t;
            while (recv(socketFd_, buf, sizeof(buf), 0) > 0) {
                // drop unused data
            }
            auto duration = t.touchMs();
            LOG_INFO("flush recv duration:{}", duration);
            socketClose();
        }
    }
}

}  // namespace libobsensor
