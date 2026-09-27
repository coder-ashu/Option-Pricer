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



# System Architecture

┌─────────────────┐
│ Python Script   │ fetches active options
│ (REST API)      │ ───> symbols.json (Loaded at startup)
└─────────────────┘

      [ NETWORK INGESTION / PRODUCER THREADS ]
┌─────────────────────────┐       ┌─────────────────────────┐
│ SpotPriceFetcher        │       │ DeribitDataFetcher      │
│ (Thread 1)              │       │ (Thread 2)              │
│ Sub: ticker.BTC-PERP    │       │ Sub: book.<options>.raw │
└───────────┬─────────────┘       └───────────┬─────────────┘
            │                                 │
            │ std::atomic store               │ lock-free emplace()
            ▼                                 ▼
┌─────────────────────────┐       ┌─────────────────────────┐
│ std::atomic<double>     │       │ L3Queue (SPSC Ring)     │
│ underlying_spot_price_  │       │ 10,240 Event Capacity   │
└───────────┬─────────────┘       └───────────┬─────────────┘
            │                                 │
            │ std::atomic load                │ lock-free pop()
            ▼                                 ▼
      [ PRICING & LOGIC / CONSUMER THREAD ]
┌───────────────────────────────────────────────────────────┐
│ TradingEngine (Thread 3)                                  │
│                                                           │
│ 1. Routing: Updates specific symbol's OrderBook.          │
│ 2. State: Extracts Option Mid-Price from OrderBook.       │
│ 3. State: Reads underlying_spot_price_ (Lock-free).       │
│ 4. Compute: Newton-Raphson IV Solver (Warm Started).      │
│ 5. Compute: Black-Scholes Greeks (Delta, Gamma, Vega).    │
└───────────────────────────────────────────────────────────┘