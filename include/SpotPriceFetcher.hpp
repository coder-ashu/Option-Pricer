#ifndef SPOT_PRICE_FETCHER_HPP
#define SPOT_PRICE_FETCHER_HPP

#include "TradingEngine.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <iomanip>
#include <thread>
#include <atomic>

namespace net = boost::asio;
namespace ssl = net::ssl;
namespace websocket = boost::beast::websocket;
using tcp = net::ip::tcp;
using json = nlohmann::json;

class SpotPriceFetcher {
private:
    std::string host_;
    std::string port_;
    std::string instrument_; // e.g., "BTC-PERPETUAL"
    TradingEngine& engine_;

    std::atomic<bool> is_running_{false};
    std::thread worker_thread_;

    void run_wss_loop() {
        try {
            net::io_context ioc;
            ssl::context ctx{ssl::context::tlsv12_client};
            ctx.set_default_verify_paths();

            tcp::resolver resolver{ioc};
            websocket::stream<boost::beast::ssl_stream<tcp::socket>> ws{ioc, ctx};

            auto const results = resolver.resolve(host_, port_);
            net::connect(ws.next_layer().next_layer(), results);

            if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host_.c_str())) {
                throw boost::beast::system_error(
                    boost::beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()));
            }
            ws.next_layer().handshake(ssl::stream_base::client);
            ws.handshake(host_, "/ws/api/v2");

            // Subscribe to the perpetual ticker feed
            std::string channel = "ticker." + instrument_ + ".100ms";
            json sub_request = {
                {"jsonrpc", "2.0"},
                {"id", 84},
                {"method", "public/subscribe"},
                {"params", {{"channels", json::array({channel})}}}
            };

            ws.write(net::buffer(sub_request.dump()));

            boost::beast::flat_buffer buffer;
            ws.read(buffer); // Subscription ACK
            const std::string ack = boost::beast::buffers_to_string(buffer.data());
            std::cout << "[SPOT SUBSCRIBE ACK] " << ack << "\n";
            auto ack_json = json::parse(ack, nullptr, false);
            if (ack_json.is_discarded() || ack_json.contains("error")) {
                std::cerr << "[SPOT SUBSCRIBE ERROR] Invalid or rejected subscription response.\n";
            } else {
                std::cout << "[SPOT CONNECTED] Subscribed to " << channel << ". Waiting for updates...\n";
            }
            buffer.clear();

            while (is_running_) {
                ws.read(buffer);
                std::string msg = boost::beast::buffers_to_string(buffer.data());
                buffer.clear();

                auto parsed = json::parse(msg, nullptr, false);
                if (parsed.is_discarded() || !parsed.contains("params")) continue;

                // Extract real-time Mark Price or Last Price
                const auto& data = parsed["params"]["data"];
                if (data.contains("mark_price") && data["mark_price"].is_number()) {
                    const double spot = data["mark_price"].get<double>();
                    engine_.set_underlying_spot(spot);
                    std::cout << "[SPOT TICK] " << instrument_ << " Spot Price Updated: $"
                              << std::fixed << std::setprecision(2) << spot << "\n";
                } else if (data.contains("last_price") && data["last_price"].is_number()) {
                    const double spot = data["last_price"].get<double>();
                    engine_.set_underlying_spot(spot);
                    std::cout << "[SPOT TICK] " << instrument_ << " Last Price Updated: $"
                              << std::fixed << std::setprecision(2) << spot << "\n";
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[SPOT FETCHER ERROR]: " << e.what() << "\n";
        }
    }

public:
    SpotPriceFetcher(const std::string& host, const std::string& port, 
                     const std::string& instrument, TradingEngine& engine)
        : host_(host), port_(port), instrument_(instrument), engine_(engine) {}

    ~SpotPriceFetcher() { stop(); }

    void start() {
        is_running_ = true;
        worker_thread_ = std::thread(&SpotPriceFetcher::run_wss_loop, this);
    }

    void stop() {
        if (is_running_) {
            is_running_ = false;
            if (worker_thread_.joinable()) worker_thread_.join();
        }
    }
};

#endif // SPOT_PRICE_FETCHER_HPP
