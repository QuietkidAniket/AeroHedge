#include <iostream>
#include <thread>
#include <pthread.h>
#include "market_data.hpp"
#include "spsc_queue.hpp"
#include "udp_listener.hpp"
#include "risk_engine.hpp"
#include "order_gateway.hpp"
#include "time_utils.hpp"
#include "telemetry.hpp" 
#include <csignal>

aerohedge::SPSCQueue<aerohedge::MarketTick, 1024> market_queue;
aerohedge::SPSCQueue<aerohedge::OrderRequest, 1024> outbound_queue;
aerohedge::SPSCQueue<aerohedge::TelemetryPacket, 1024> metrics_queue; 

aerohedge::TSCClock aerohedge::global_clock;

void pin_thread_to_core(std::thread& th, int core_id) {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(th.native_handle(), sizeof(cpu_set_t), &cpuset);
#else
    std::cout << "[System] Non-Linux environment detected. Skipping hard affinity constraints for core: " << core_id << "\n";
#endif
}

void network_ingestion_loop() {
    aerohedge::UdpListener listener("239.255.0.1", 9000);
    listener.listen_and_publish(market_queue); 
}

void risk_execution_loop() {
    // <-- Passed metrics_queue into the engine
    aerohedge::RiskEngine engine(100.0, 0.082, 0.05, 0.20, 5.0, outbound_queue, metrics_queue);
    
    aerohedge::MarketTick tick;
    while (true) {
        if (market_queue.pop(tick)) {
            engine.process_tick(tick);
        }
    }
}

void order_transmission_loop() {
    aerohedge::OrderGateway gateway("127.0.0.1", 9999);
    gateway.connect_to_exchange();
    gateway.outbound_loop(outbound_queue);
}

// <-- NEW: Thread 4: Telemetry Broadcaster
void telemetry_loop() {
    std::cout << "[TELEMETRY]: Broadcaster starting on 127.0.0.1:8080...\n";
    aerohedge::TelemetryBroadcaster broadcaster("127.0.0.1", 8080);
    aerohedge::TelemetryPacket packet;
    
    while (true) {
        if (metrics_queue.pop(packet)) {
            broadcaster.broadcast(packet);
        }
    }
}

int main() {
    // Tells the OS not to kill our engine if the TCP connection drops
    signal(SIGPIPE, SIG_IGN);
    std::cout << "[MAIN]: Starting AeroHedge High-Frequency Execution Subsystem...\n";

    std::thread ingestion_th(network_ingestion_loop);
    std::thread execution_th(risk_execution_loop);
    std::thread transmission_th(order_transmission_loop);
    std::thread telemetry_th(telemetry_loop); // <-- Spawned Thread 4

    pin_thread_to_core(ingestion_th, 1);
    pin_thread_to_core(execution_th, 2);
    pin_thread_to_core(transmission_th, 3);
    pin_thread_to_core(telemetry_th, 4); // <-- Pinned Thread 4

    ingestion_th.join();
    execution_th.join();
    transmission_th.join();
    telemetry_th.join();

    return 0;
}