#include "FetchData.hpp"
#include "TradingEngine.hpp"
#include <iostream>


std::vector<std::string> load_symbols_from_json(const std::string& filename) {
    std::ifstream file(filename);
    nlohmann::json j;
    file >> j;
    return j["instruments"].get<std::vector<std::string>>();
}

int main() {
    try {

        // load symbols from config file

        std::vector<std::string> symbols = load_symbols_from_json("../config/symbols.json");
        // 1. Create the lock-free queue (Shared Memory between threads)
        L3Queue queue;

        // 2. Initialize Producer (WebSocket Fetcher)
        std::string host = "test.deribit.com"; // Use testnet for development
        std::string port = "443";
        std::string instrument = "BTC-27SEP26-60000-C"; 
        
        DeribitDataFetcher fetcher(host, port, instrument, queue);

        // 3. Initialize Consumer (Trading Engine / Pricer)
        TradingEngine engine(queue, fetcher);

        // 4. Start the threads!
        std::cout << "Starting Trading Engine Consumer Thread...\n";
        engine.start();

        std::cout << "Starting Deribit WebSocket Producer Thread...\n";
        fetcher.start();

        // Let it run until user presses Enter
        std::cout << "Press [ENTER] to stop streaming...\n";
        std::cin.get();

        // 5. Clean Shutdown
        std::cout << "Shutting down...\n";
        fetcher.stop();
        engine.stop();

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
    }

    return 0;
}