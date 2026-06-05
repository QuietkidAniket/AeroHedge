#pragma once
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "spsc_queue.hpp"

namespace aerohedge {

// Outbound TCP Order gateway

// Packed 24-byte binary structure matching the exchange execution interface
struct OrderRequest {
    uint64_t client_order_id; // 8 bytes
    double   price;           // 8 bytes
    uint32_t instrument_id;   // 4 bytes
    int32_t  quantity;        // 4 bytes (positive = BUY, negative = SELL)
};

class OrderGateway {
private:
    int socket_fd_;
    struct sockaddr_in exchange_addr_;

public:
    OrderGateway(const char* exchange_ip, int port) {
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ < 0) {
            std::cerr << "[ORDER GATEWAY]: Failed to create TCP socket.\n";
            exit(1);
        }

        // Set socket to non-blocking mode to ensure the gateway thread never hangs on kernel calls
        int flags = fcntl(socket_fd_, F_GETFL, 0);
        fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);

        memset(&exchange_addr_, 0, sizeof(exchange_addr_));
        exchange_addr_.sin_family = AF_INET;
        exchange_addr_.sin_port = htons(port);
        exchange_addr_.sin_addr.s_addr = inet_addr(exchange_ip);
    }

    ~OrderGateway() {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
        }
    }

    bool connect_to_exchange() {
        int rc = connect(socket_fd_, (struct sockaddr*)&exchange_addr_, sizeof(exchange_addr_));
        if (rc < 0) {
            if (errno == EINPROGRESS) {
                // Expected behavior for non-blocking TCP connection requests
                return true; 
            }
            std::cerr << "[ORDER GATEWAY]: TCP Connection failed]: " << strerror(errno) << "\n";
            return false;
        }
        return true;
    }

    // Run-loop execution for the dedicated Outbound Gateway Thread
    void outbound_loop(SPSCQueue<OrderRequest, 1024>& outbound_queue) {
        OrderRequest order;
        while (true) {
            if (outbound_queue.pop(order)) {
                // MSG_DONTWAIT guarantees immediate execution or failure back to user space
                ssize_t bytes_sent = send(socket_fd_, &order, sizeof(OrderRequest), MSG_DONTWAIT);
                
                if (bytes_sent < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // Handle TCP backpressure / socket buffer overflow
                        // Spin hot until space opens up on the socket transmission buffer
                        while (send(socket_fd_, &order, sizeof(OrderRequest), MSG_DONTWAIT) < 0) {
                            // High-frequency spin retry
                        }
                    } else {
                        std::cerr << "[Gateway]: Socket exception: " << strerror(errno) << "\n";
                    }
                }
            }
        }
    }
};

} // namespace aerohedge