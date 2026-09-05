#This is the  1st version of the project , optimisations would follow 


options-pricer/
├── CMakeLists.txt
├── include/
│   ├── OrderBook.hpp        # B-Tree or map-based limit order book state
│   ├── BlackScholes.hpp     # The statistical math core (pricing & Greeks)
│   ├── MarketData.hpp       # CSV parser and event generator
│   └── Engine.hpp           # The main event loop bridging data and math
├── src/
│   ├── OrderBook.cpp
│   ├── BlackScholes.cpp
│   ├── MarketData.cpp
│   ├── Engine.cpp
│   └── main.cpp             # Entry point
└── data/
    └── historical_ticks.csv # Your offline data feed