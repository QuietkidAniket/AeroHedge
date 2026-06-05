#pragma once
#include "market_data.hpp"
#include "order_gateway.hpp"
#include "spsc_queue.hpp"
#include "telemetry.hpp" 
#include "time_utils.hpp"
#include <cmath>

namespace aerohedge {
  
class RiskEngine {
private:
    double strike_;
    double time_to_expiry_;
    double risk_free_rate_;
    double volatility_;
    
    int current_position_;  
    double cash_;           
    double delta_threshold_;
    uint64_t order_id_counter_;

    uint64_t latency_records_[10000]; 
    int trade_count_ = 0; 
    double last_latency_ns_ = 0.0; 

    SPSCQueue<OrderRequest, 1024>& outbound_queue_;
    SPSCQueue<TelemetryPacket, 1024>& metrics_queue_; 

    inline double fast_cdf(double x) const {
        const double a1 =  0.254829592; const double a2 = -0.284496736;
        const double a3 =  1.421413741; const double a4 = -1.453152027;
        const double a5 =  1.061405429; const double p  =  0.3275911;
        int sign = (x < 0) ? -1 : 1;
        x = std::abs(x) / std::sqrt(2.0);
        double t = 1.0 / (1.0 + p * x);
        double y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * std::exp(-x * x);
        return 0.5 * (1.0 + sign * y);
    }

public:
    // Constructor updated to take the metrics queue and initialize cash
    RiskEngine(double k, double tte, double r, double vol, double thresh, 
               SPSCQueue<OrderRequest, 1024>& out_q,
               SPSCQueue<TelemetryPacket, 1024>& met_q) 
        : strike_(k), time_to_expiry_(tte), risk_free_rate_(r), volatility_(vol), 
          current_position_(0), cash_(0.0), delta_threshold_(thresh), order_id_counter_(1), 
          outbound_queue_(out_q), metrics_queue_(met_q) {}

    void process_tick(const MarketTick& tick) {
        double d1 = (std::log(tick.price / strike_) + 
                    (risk_free_rate_ + 0.5 * volatility_ * volatility_) * time_to_expiry_) / 
                    (volatility_ * std::sqrt(time_to_expiry_));
        
        double target_delta = fast_cdf(d1);
        int target_shares = static_cast<int>(target_delta * 100);
        int hedge_diff = target_shares - current_position_;
        
        if (std::abs(hedge_diff) >= delta_threshold_) {
            OrderRequest req{
                order_id_counter_++,
                tick.price,
                tick.instrument_id,
                hedge_diff
            };
            
            uint64_t egress_cycles = global_clock.rdtsc();
            uint64_t cycle_diff = egress_cycles - tick.ingress_cycles;
            last_latency_ns_ = global_clock.cycles_to_ns(cycle_diff);
            
            if (trade_count_ < 10000) {
                latency_records_[trade_count_++] = last_latency_ns_;
            }

            while (!outbound_queue_.push(req)) {}
            
            // Update position and cash (If we buy, cash goes down. If we sell, cash goes up)
            current_position_ += hedge_diff; 
            cash_ -= (hedge_diff * tick.price);
        }

        // FIRE AND FORGET TELEMETRY 
        TelemetryPacket tp;
        tp.timestamp = tick.timestamp;
        tp.current_price = tick.price;
        tp.current_delta = target_delta;
        tp.position = current_position_;
        // Total Unrealized P&L = Cash + (Current Inventory Value)
        tp.pnl = static_cast<int32_t>(cash_ + (current_position_ * tick.price));
        tp.last_latency = last_latency_ns_;
        
        // We use a non-blocking push here. If the UI thread falls behind, we just drop the 
        // telemetry frame. We NEVER allow the UI to stall the critical path.
        metrics_queue_.push(tp); 
    }
};

} // namespace aerohedge