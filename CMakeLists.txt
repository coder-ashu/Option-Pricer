cmake_minimum_required(VERSION 3.20)
project(VortexPricer VERSION 1.0 LANGUAGES CXX)

# ---------------------------------------------------------
# 1. Standard Enforcement
# ---------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------
# 2. Performance & Warning Flags
# ---------------------------------------------------------
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wshadow -O3 -march=native")

# ---------------------------------------------------------
# 3. External Dependencies & Include Directories
# ---------------------------------------------------------
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Shared linking targets
set(NET_LIBRARIES
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# ---------------------------------------------------------
# 4. Target 1: Standalone Data Test Executable
# ---------------------------------------------------------
add_executable(data_test  src/data_test.cpp src/FetchData.cpp)
target_link_libraries(data_test PRIVATE ${NET_LIBRARIES})

# ---------------------------------------------------------
# 5. Target 2: Main Engine Executable
# ---------------------------------------------------------
set(ENGINE_SOURCES
    src/main.cpp
    src/BlackScholes.cpp
    src/MarketData.cpp
    src/OrderBook.cpp
    src/Engine.cpp
    src/PolygonMarketData.cpp
    src/SimulatedBroker.cpp
    src/FetchData.cpp
)

add_executable(option_pricer ${ENGINE_SOURCES})
target_link_libraries(option_pricer PRIVATE ${NET_LIBRARIES} m)