#include "FetchData.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>
#include <iostream>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;
using json = nlohmann::json;

DeribitDataFetcher::DeribitDataFetcher(const std::string& host, const std::string& port,
                                       const std::string& instrument, L3Queue& queue)
    : host_(host), port_(port), instrument_(instrument), queue_(queue) {
    
    // Parse once when object is created!
    parse_instrument();

    if (is_option_) {
        std::cout << "[INFO] Parsed Option: Strike=" << strike_price_ 
                  << ", Type=" << (is_call_ ? "Call" : "Put") 
                  << ", Expiry=" << expiry_date_ << "\n";
    }
}


DeribitDataFetcher::~DeribitDataFetcher() {
    stop();
}


void DeribitDataFetcher::parse_instrument() {
    std::stringstream ss(instrument_);
    std::string token;
    std::vector<std::string> parts;

    // Split "BTC-27SEP26-60000-C" by '-'
    while (std::getline(ss, token, '-')) {
        parts.push_back(token);
    }

    
    if (parts.size() >= 4) {
        is_option_ = true;
        expiry_date_ = parts[1];                // "27SEP26"
        strike_price_ = std::stod(parts[2]);    // 60000.0
        is_call_ = (parts[3] == "C");            // true if "C"
    }
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

        // Build JSON-RPC channels array
        json channels = json::array();
        for (const auto& symbol : instruments_) {
            channels.push_back("book." + symbol + ".raw");
        }

        json sub_request = {
            {"jsonrpc", "2.0"},
            {"id", 42},
            {"method", "public/subscribe"},
            {"params", {{"channels", channels}}} // Batch subscribe to all 10 symbols!
        };

        ws.write(net::buffer(sub_request.dump()));

        beast::flat_buffer buffer;
        ws.read(buffer); // Subscription ACK
        buffer.clear();

        while (is_running_) {
            ws.read(buffer);
            std::string msg = beast::buffers_to_string(buffer.data());
            buffer.clear();

            auto parsed = json::parse(msg, nullptr, false);
            if (parsed.is_discarded() || !parsed.contains("params")) continue;

            const auto& data = parsed["params"]["data"];
            L3BookUpdateEvent<64> event{};
            event.timestamp = data.value("timestamp", 0ULL);
            event.change_id = data.value("change_id", 0ULL);

            std::string type = data.value("type", "change");
            BookAction default_action = (type == "snapshot") ? BookAction::SNAPSHOT : BookAction::CHANGE;

            auto parse_side = [&](const json& entries, BookSide side) {
                for (const auto& item : entries) {
                    if (!item.is_array() || item.size() < 3) continue;
                    std::string act_str = item[0].get<std::string>();
                    double price = item[1].get<double>();
                    double amount = item[2].get<double>();

                    BookAction act = default_action;
                    if (act_str == "new") act = BookAction::NEW;
                    else if (act_str == "change") act = BookAction::CHANGE;
                    else if (act_str == "delete") act = BookAction::DELETE;

                    event.addDelta(act, side, price, amount);
                }
            };

            if (data.contains("bids")) parse_side(data["bids"], BookSide::BID);
            if (data.contains("asks")) parse_side(data["asks"], BookSide::ASK);

            // Emplace directly into SPSC ring buffer
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