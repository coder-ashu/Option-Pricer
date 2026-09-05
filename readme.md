#This is the  1st version of the project , optimisations would follow 


options-pricer/
├── CMakeLists.txt              # [MODIFIED] Link against IBKR TwsApi library
├── config/
│   └── paper_config.json       # [NEW] Socket ports, contract symbols, risk limits
├── include/
│   ├── BlackScholes.hpp        # Unchanged (Pure math core)
│   ├── OrderBook.hpp          # [MODIFIED] Support streaming bid/ask updates
│   ├── MarketData.hpp          # [MODIFIED] Refactor into abstract IMarketDataProvider
│   ├── IBMarketData.hpp        # [NEW] Real-time tick stream from IBKR
│   ├── IBExecutionHandler.hpp  # [NEW] Inherits EWrapper/EClientSocket for API calls
│   ├── RiskManager.hpp         # [NEW] Aggregates Net Delta/Gamma & enforces Kill Switches
│   ├── LockFreeQueue.hpp       # [NEW] SPSC Queue to pass ticks from API to engine
│   └── Engine.hpp              # [MODIFIED] Event loop consuming lock-free thread queue
├── src/
│   ├── OrderBook.cpp
│   ├── BlackScholes.cpp
│   ├── MarketData.cpp
│   ├── IBMarketData.cpp        # [NEW] IBKR tick parser
│   ├── IBExecutionHandler.cpp  # [NEW] EWrapper callback implementation
│   ├── RiskManager.cpp         # [NEW] Hedging logic & fat-finger filters
│   ├── Engine.cpp              # [MODIFIED] Multi-threaded processing loop
│   └── main.cpp                # [MODIFIED] App entry point, signal handler, thread init
└── data/
    └── historical_ticks.csv    # Kept for offline regression testing