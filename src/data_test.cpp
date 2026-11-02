#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;
using json = nlohmann::json;

// Helper to fetch the first active BTC option contract from Deribit Testnet REST API
std::string get_first_active_option_symbol() {
    try {
        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};
        ctx.set_default_verify_paths();

        tcp::resolver resolver{ioc};
        beast::ssl_stream<beast::tcp_stream> stream{ioc, ctx};

        const std::string host = "test.deribit.com";
        if(!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
            throw beast::system_error(beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()));

        auto const results = resolver.resolve(host, "443");
        beast::get_lowest_layer(stream).connect(results);
        stream.handshake(ssl::stream_base::client);

        // Deribit REST API endpoint to query active BTC option instruments
        http::request<http::string_body> req{http::verb::get, "/api/v2/public/get_instruments?currency=BTC&kind=option", 11};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        http::write(stream, req);

        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);

        json parsed = json::parse(res.body());
        if (parsed.contains("result") && !parsed["result"].empty()) {
            std::string active_symbol = parsed["result"][0]["instrument_name"].get<std::string>();
            return active_symbol;
        }
    } catch (const std::exception& e) {
        std::cerr << "[REST FETCH ERROR]: " << e.what() << "\n";
    }
    // Fallback perpetual instrument if options query fails
    return "BTC-PERPETUAL";
}

int main() {
    try {
        const std::string host = "test.deribit.com";
        const std::string port = "443";
        const std::string endpoint = "/ws/api/v2";

        // 1. Fetch valid active contract dynamically
        std::string active_instrument = get_first_active_option_symbol();
        std::string channel = "ticker." + active_instrument + ".100ms";

        std::cout << "[DYNAMIC SYMBOL] Selected Active Channel: " << channel << "\n";

        // 2. Establish WSS Stream
        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};
        ctx.set_default_verify_paths();

        tcp::resolver resolver{ioc};
        websocket::stream<beast::ssl_stream<tcp::socket>> ws{ioc, ctx};

        auto const results = resolver.resolve(host, port);
        net::connect(ws.next_layer().next_layer(), results);

        if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str())) {
            throw beast::system_error(beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()));
        }
        ws.next_layer().handshake(ssl::stream_base::client);
        ws.handshake(host, endpoint);

        std::cout << "[CONNECTED] Secure WSS Session Established!\n";

        // 3. Send Subscription Request
        json sub_request = {
            {"jsonrpc", "2.0"},
            {"id", 42},
            {"method", "public/subscribe"},
            {"params", {
                {"channels", json::array({channel})}
            }}
        };

        ws.write(net::buffer(sub_request.dump()));

        beast::flat_buffer buffer;
        ws.read(buffer);
        std::cout << "[SUB ACK]: " << beast::buffers_to_string(buffer.data()) << "\n";
        std::cout << "---------------------------------------------------------\n";
        buffer.clear();

        // 4. Stream Quotes
        while (true) {
            ws.read(buffer);
            std::cout << "[TICK]: " << beast::buffers_to_string(buffer.data()) << "\n";
            buffer.clear();
        }

    } catch (const std::exception& e) {
        std::cerr << "[ERROR]: " << e.what() << "\n";
        return 1;
    }
    return 0;
}