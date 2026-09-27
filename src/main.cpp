#include "FetchData.hpp"
#include "SpotPriceFetcher.hpp"
#include "TradingEngine.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>

// Helper to load instruments from JSON
std::vector<std::string> load_symbols(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) { file.clear(); file.open("config/symbols.json"); }
    if (!file.is_open()) { file.clear(); file.open("../config/symbols.json"); }
    if (!file.is_open()) {
        throw std::runtime_error("Could not open config/symbols.json from the working directory.");
    }
    nlohmann::json j;
    file >> j;
    if (!j.is_object() || !j.contains("instruments") || !j["instruments"].is_array()) {
        throw std::runtime_error("config/symbols.json must contain an instruments array.");
    }
    return j["instruments"].get<std::vector<std::string>>();
}

int main() {
    try {
        // 1. Configuration
        std::string host = "test.deribit.com";
        std::string port = "443";
        std::string spot_instrument = "BTC-PERPETUAL";
        std::cout << "[CONFIG] Deribit endpoint: " << host << ":" << port
                  << "; spot instrument: " << spot_instrument << "\n";

        // Path to config folder relative to binary execution directory
        std::vector<std::string> symbols = load_symbols("../config/symbols.json");
        std::cout << "[INIT] Loaded " << symbols.size() << " option contracts.\n";

        // 2. Shared Architecture Setup (Heap-allocated to prevent Stack Overflow)
        auto queue_ptr = std::make_unique<L3Queue>();
        L3Queue& queue = *queue_ptr;

        TradingEngine engine(queue, symbols);

        // 3. Network Fetchers
        DeribitDataFetcher options_fetcher(host, port, symbols, queue);
        SpotPriceFetcher spot_fetcher(host, port, spot_instrument, engine);

        // 4. Start all threads
        std::cout << "[STARTING] Trading Engine (Consumer)...\n";
        engine.start();

        std::cout << "[STARTING] Spot Price Fetcher (Producer)...\n";
        spot_fetcher.start();

        std::cout << "[STARTING] Options L3 Fetcher (Producer)...\n";
        options_fetcher.start();

        // 5. Keep main thread alive
        std::cout << "\n>>> System is running. Press [ENTER] to gracefully shutdown. <<<\n\n";
        std::cin.get();

        // 6. Orderly Shutdown
        std::cout << "[SHUTDOWN] Stopping network fetchers...\n";
        options_fetcher.stop();
        spot_fetcher.stop();

        std::cout << "[SHUTDOWN] Stopping trading engine...\n";
        engine.stop();
        std::cout << "[SHUTDOWN] Complete.\n";

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error in main: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
