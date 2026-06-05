#pragma once
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace aerohedge {

// Packed struct for telemetry (Total: 40 bytes)
struct TelemetryPacket {
    uint64_t timestamp;      // For the X-axis of our charts
    double   current_price;  // The simulated stock price
    double   current_delta;  // Option Delta (0.0 to 1.0)
    int32_t  position;       // Current inventory
    int32_t  pnl;            // Profit & Loss
    double   last_latency;   // The p99 or raw latency of the last trade
};

class TelemetryBroadcaster {
private:
    int socket_fd_;
    struct sockaddr_in monitor_addr_;

public:
    TelemetryBroadcaster(const char* monitor_ip, int port) {
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        
        memset(&monitor_addr_, 0, sizeof(monitor_addr_));
        monitor_addr_.sin_family = AF_INET;
        monitor_addr_.sin_port = htons(port);
        monitor_addr_.sin_addr.s_addr = inet_addr(monitor_ip);
    }

    ~TelemetryBroadcaster() {
        close(socket_fd_);
    }

    // Call this from your Telemetry Thread 
    // (reading from the metrics SPSC queue we discussed)
    inline void broadcast(const TelemetryPacket& packet) {
        sendto(socket_fd_, &packet, sizeof(TelemetryPacket), MSG_DONTWAIT,
               (struct sockaddr*)&monitor_addr_, sizeof(monitor_addr_));
    }
};

} // namespace aerohedge