#pragma once
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "market_data.hpp"
#include "spsc_queue.hpp"
#include "time_utils.hpp"

namespace aerohedge {
  
  class UdpListener {
    private:
    int socket_fd_;
    struct sockaddr_in addr_;

public:
    UdpListener(const char* multicast_ip, int port) {
        // 1. Create a UDP socket
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            std::cerr << "[UDP Listener]: Failed to create socket.\n";
            exit(1);
        }

        // 2. Allow multiple listeners on the same port (crucial for local testing)
        int reuse = 1;
        setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        // 3. Bind the socket to the port
        memset(&addr_, 0, sizeof(addr_));
        addr_.sin_family = AF_INET;
        addr_.sin_addr.s_addr = htonl(INADDR_ANY);
        addr_.sin_port = htons(port);

        if (bind(socket_fd_, (struct sockaddr*)&addr_, sizeof(addr_)) < 0) {
            std::cerr << "[UDP Listener]: Bind failed.]\n";
            exit(1);
        }

        // 4. Join the Multicast Group
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(multicast_ip);
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        setsockopt(socket_fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    }

    ~UdpListener() {
        close(socket_fd_);
    }

    // This runs in a tight loop on our pinned Network Thread
    void listen_and_publish(SPSCQueue<MarketTick, 1024>& queue) {
        MarketTick tick; // Pre-allocated stack memory

        while (true) {
            ssize_t bytes_read = recv(socket_fd_, &tick, sizeof(MarketTick) - sizeof(uint64_t), 0);
            
            if (bytes_read > 0) {
                // Tag the hardware cycle EXACTLY after the packet hits user space
                tick.ingress_cycles = global_clock.rdtsc(); 
                
                while (!queue.push(tick)) {}
            }
        }
    }
};

} // namespace aerohedge