#include "FetchData.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstring>
#include <unordered_set>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;
using json = nlohmann::json;

DeribitDataFetcher::DeribitDataFetcher(const std::string& host, const std::string& port,
                                       const std::vector<std::string>& instruments, L3Queue& queue)
    : host_(host), port_(port), instruments_(instruments), queue_(queue) {}

DeribitDataFetcher::~DeribitDataFetcher() {
    stop();
}

void DeribitDataFetcher::start() {
    is_running_ = true;
    worker_thread_ = std::thread(&DeribitDataFetcher::run_wss_loop, this);
}

void DeribitDataFetcher::stop() {
    if (is_running_) {
        is_running_ = false;
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
}

void DeribitDataFetcher::run_wss_loop() {
    try {
        std::cout << "[OPTIONS CONNECTING] Connecting to " << host_ << ":" << port_ << "...\n";
        std::unordered_set<std::string> first_update_reported;
        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};
        ctx.set_default_verify_paths();

        tcp::resolver resolver{ioc};
        websocket::stream<beast::ssl_stream<tcp::socket>> ws{ioc, ctx};

        auto const results = resolver.resolve(host_, port_);
        net::connect(ws.next_layer().next_layer(), results);

        if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host_.c_str())) {
            throw beast::system_error(beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()));
        }
        ws.next_layer().handshake(ssl::stream_base::client);
        ws.handshake(host_, "/ws/api/v2");

        // 1. Construct JSON-RPC batch channel array for all instruments
        json channels = json::array();
        for (const auto& symbol : instruments_) {
            channels.push_back("book." + symbol + ".none.20.100ms");
        }
    
        json sub_request = {
            {"jsonrpc", "2.0"},
            {"id", 42},
            {"method", "public/subscribe"},
            {"params", {{"channels", channels}}}
        };

        ws.write(net::buffer(sub_request.dump()));

        beast::flat_buffer buffer;
        ws.read(buffer); // Subscription ACK
        const std::string ack = beast::buffers_to_string(buffer.data());
        std::cout << "[OPTIONS SUBSCRIBE ACK] " << ack << "\n";
        auto ack_json = json::parse(ack, nullptr, false);
        if (ack_json.is_discarded() || ack_json.contains("error")) {
            std::cerr << "[OPTIONS SUBSCRIBE ERROR] Invalid or rejected subscription response.\n";
        }
        buffer.clear();
        std::cout << "[OPTIONS CONNECTED] Subscribed to " << instruments_.size()
                  << " order book channels. Waiting for updates...\n";

        // 2. High-frequency streaming loop
        while (is_running_) {
            ws.read(buffer);
            std::string msg = beast::buffers_to_string(buffer.data());
            buffer.clear();

            auto parsed = json::parse(msg, nullptr, false);
            if (parsed.is_discarded()) {
                std::cerr << "[OPTIONS PARSE ERROR] Could not parse websocket message.\n";
                continue;
            }
            if (parsed.contains("error")) {
                std::cerr << "[OPTIONS API ERROR] " << parsed["error"].dump() << "\n";
                continue;
            }
            if (!parsed.contains("params")) continue;

            const auto& params = parsed["params"];
            std::string channel_name = params.value("channel", "");
            if (!params.contains("data") || !params["data"].is_object()) continue;
            const auto& data = params["data"];

            // --- FIXED STRING EXTRACTION LOGIC ---
            std::string symbol = "";
            if (channel_name.rfind("book.", 0) == 0) {
                size_t start = 5; // length of "book."
                size_t end = channel_name.find(".none.");
                if (end != std::string::npos && end > start) {
                    symbol = channel_name.substr(start, end - start);
                }
            }
            if (symbol.empty()) {
                symbol = data.value("instrument_name", "");
            }

            // --- FIXED BUFFER OVERFLOW & MEMORY SAFETY ---
            if (symbol.empty() || symbol.size() >= 32) {
                // Drop if symbol is empty or too large for the 32-byte char array
                continue;
            }

            L3BookUpdateEvent<64> event{};
            std::memset(event.symbol, 0, sizeof(event.symbol)); // Zero out memory
            std::strncpy(event.symbol, symbol.c_str(), sizeof(event.symbol) - 1); // Safe copy
            
            event.timestamp = data.value("timestamp", 0ULL);
            event.change_id = data.value("change_id", 0ULL);

            std::string type = data.value("type", "change");
            
            // NOTE: Ensure your L3BookUpdateEvent struct in FetchData.hpp has the "bool is_snapshot;" member!
            event.is_snapshot = (type == "snapshot");
            BookAction default_action = event.is_snapshot ? BookAction::SNAPSHOT : BookAction::CHANGE;

            auto parse_side = [&](const json& entries, BookSide side) {
            for (const auto& item : entries) {
                // Public depth channels send [price, amount] (size 2)
                if (!item.is_array() || item.size() < 2) continue;
                
                // Safely extract price and amount
                double price = item[0].is_number() ? item[0].get<double>() : 0.0;
                double amount = item[1].is_number() ? item[1].get<double>() : 0.0;

                if (price <= 0.0) continue;

                // In public depth channels, an amount of 0 means the price level was deleted.
                BookAction act = BookAction::CHANGE;
                if (amount == 0.0) {
                    act = BookAction::DELETE;
                } else if (event.is_snapshot) {
                    act = BookAction::NEW;
                }

                event.addDelta(act, side, price, amount);
            }
        };

            if (data.contains("bids")) parse_side(data["bids"], BookSide::BID);
            if (data.contains("asks")) parse_side(data["asks"], BookSide::ASK);

            if (event.is_snapshot) {
                std::cout << "[BOOK SNAPSHOT] " << symbol << " bids="
                          << (data.contains("bids") ? data["bids"].size() : 0)
                          << " asks=" << (data.contains("asks") ? data["asks"].size() : 0) << "\n";
            } else if (first_update_reported.insert(symbol).second) {
                std::cout << "[BOOK STREAMING] First incremental update received for " << symbol << "\n";
            }

            // Lock-free push to SPSC Ring Buffer
            while (!queue_.emplace(event) && is_running_) {
                #if defined(__x86_64__) || defined(_M_X64)
                _mm_pause();
                #endif
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[FETCHER ERROR]: " << e.what() << "\n";
    }
}