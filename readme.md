# this is v0


options-pricer/
├── CMakeLists.txt              # [MODIFIED] Link Boost (Asio/Beast), OpenSSL, & Threads
├── config/
│   └── paper_config.json     # [MODIFIED] API Key, WebSocket endpoint, target symbols
├── external/
│   └── nlohmann/               # [NEW] Header-only JSON parser (nlohmann/json.hpp)
├── include/
│   ├── BlackScholes.hpp        # Unchanged (Pure math core)
│   ├── OrderBook.hpp           # [MODIFIED] Support Polygon JSON Quote/Trade updates
│   ├── MarketData.hpp          # Abstract Interface (IMarketDataProvider)
│   ├── PolygonMarketData.hpp   # [REPLACED IBMarketData] Boost.Beast WebSocket client
│   ├── SimulatedBroker.hpp     # [REPLACED IBExecutionHandler] Local paper trading OMS
│   ├── RiskManager.hpp         # [MODIFIED] Tracks paper positions & delta hedging limits
│   ├── LockFreeQueue.hpp       # Unchanged (SPSC Queue passing JSON strings to Engine)
│   └── Engine.hpp              # [MODIFIED] Multi-threaded JSON parser & strategy loop
├── src/
│   ├── OrderBook.cpp
│   ├── BlackScholes.cpp
│   ├── MarketData.cpp
│   ├── PolygonMarketData.cpp   # [NEW] Connection, Auth, and Subscriptions over SSL WSS
│   ├── SimulatedBroker.cpp     # [NEW] Paper order execution, mid-price fills, P&L
│   ├── RiskManager.cpp         # Risk limits & position tracking
│   ├── Engine.cpp              # Consumes SPSC queue, updates math, triggers paper orders
│   └── main.cpp                # App entry point, signal handler, thread init
└── data/
    └── historical_ticks.csv    # Kept for offline regression testing



# Data Parsing 

┌────────────────────────┐
│  Deribit Exchange      │
└───────────┬────────────┘
            │  WSS Payload (JSON over SSL)
            ▼
┌────────────────────────┐
│  DeribitDataFetcher    │  1. Boost.Beast receives WebSocket frame
│  (Worker Thread)       │  2. nlohmann::json parses array triples
└───────────┬────────────┘  3. Constructs stack L3BookUpdateEvent
            │
            │ queue_.emplace(event) [Lock-Free, zero-alloc]
            ▼
┌────────────────────────┐
│  SPSCQueue             │  Ring Buffer (Capacity 10,240)
└───────────┬────────────┘
            │ queue_.pop(event)
            ▼
┌────────────────────────┐
│  Strategy Engine       │
│  (Consumer Thread)     │  Processes book deltas for market making
└────────────────────────┘