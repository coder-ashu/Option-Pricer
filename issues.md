# Root Cause Analysis & Issues Fixed in Option Pricer

This document details the issues identified in `FetchData.cpp`, `TradingEngine.hpp`, `OrderBook.hpp`, and `data/fetch_symbol.py`, explaining why the bid and ask order book data was not being populated after connecting to Deribit, and how the flow was fixed.

---

## Architecture Flow Overview

The intended architecture follows an ingestion and consumption pipeline:
1. **Network Ingestion (`DeribitDataFetcher`)**: Connects to Deribit WebSocket API (`wss://test.deribit.com/ws/api/v2`), subscribes to order book channels, receives the initial `snapshot`, followed by incremental delta updates (`change`), and pushes `L3BookUpdateEvent`s to a lock-free SPSC queue.
2. **Network Ingestion (`SpotPriceFetcher`)**: Connects to Deribit WebSocket API, subscribes to `ticker.BTC-PERPETUAL.100ms`, and atomically stores the underlying spot price.
3. **Consumer Engine (`TradingEngine`)**: Consumes events from the SPSC queue, updates each symbol's `OrderBook`, calculates the mid-price, solves for Implied Volatility (IV) using Newton-Raphson / bisection, and computes Black-Scholes Greeks (Delta, Gamma, Vega).

---

## 1. Array Triplet Payload Parsing Bug in `FetchData.cpp` (Primary Cause)

### The Problem
In Deribit's order book WebSocket API (`book.{instrument_name}.{interval}`), both the initial `snapshot` and subsequent `change` messages format each level in `bids` and `asks` as a **3-element array (triplet)**:

```json
"bids": [
    ["new", 0.0495, 10.0],
    ["change", 0.0490, 15.0],
    ["delete", 0.0485, 0.0]
]
```
Where:
- `item[0]` is a string representing the action (`"new"`, `"change"`, or `"delete"`).
- `item[1]` is a number representing the price (`0.0495`).
- `item[2]` is a number representing the amount (`10.0`).

In the original `FetchData.cpp` code, `parse_side` was written assuming 2-element tuples `[price, amount]`:

```cpp
// ORIGINAL BUGGY CODE
auto parse_side = [&](const json& entries, BookSide side) {
    for (const auto& item : entries) {
        if (!item.is_array() || item.size() < 2) continue;

        double price = item[0].is_number() ? item[0].get<double>() : 0.0;
        double amount = item[1].is_number() ? item[1].get<double>() : 0.0;

        if (price <= 0.0) continue; // <-- EVERY ITEM DROPPED HERE!
        ...
    }
};
```

Because `item[0]` was a string (`"new"`, `"change"`, or `"delete"`), `item[0].is_number()` returned `false`. Consequently, `price` was evaluated as `0.0`, triggering `if (price <= 0.0) continue;` and **silently dropping every single bid and ask**.

### The Fix
Updated `parse_side` to dynamically support both 3-element triplets (`[action, price, amount]`) and 2-element pairs (`[price, amount]`):

```cpp
auto parse_side = [&](const json& entries, BookSide side) {
    for (const auto& item : entries) {
        if (!item.is_array() || item.size() < 2) continue;

        BookAction act = BookAction::CHANGE;
        double price = 0.0;
        double amount = 0.0;

        if (item.size() >= 3 && item[0].is_string()) {
            std::string action_str = item[0].get<std::string>();
            price = item[1].is_number() ? item[1].get<double>() : 0.0;
            amount = item[2].is_number() ? item[2].get<double>() : 0.0;

            if (action_str == "delete" || amount <= 0.0) {
                act = BookAction::DELETE;
            } else if (action_str == "new") {
                act = BookAction::NEW;
            } else {
                act = BookAction::CHANGE;
            }
        } else {
            price = item[0].is_number() ? item[0].get<double>() : 0.0;
            amount = item[1].is_number() ? item[1].get<double>() : 0.0;

            if (amount <= 0.0) {
                act = BookAction::DELETE;
            } else if (event.is_snapshot) {
                act = BookAction::NEW;
            } else {
                act = BookAction::CHANGE;
            }
        }

        if (price <= 0.0) continue;

        if (!event.addDelta(act, side, price, amount)) {
            push_event();
            event.addDelta(act, side, price, amount);
        }
    }
};
```

---

## 2. Incorrect Channel Subscription String & Fragile Symbol Extraction

### The Problem
1. **Channel Subscription**: `FetchData.cpp` was subscribing to `"book." + symbol + ".none.20.100ms"`. On Deribit, depth-limited channels (`none.20.100ms`) push periodic grouped depth snapshots (`"type"` is `null` and entries are static snapshots) rather than sending an initial snapshot followed by stream delta actions (`"type": "change"`). The standard public feed that sends `type: "snapshot"` followed by `type: "change"` is `"book." + symbol + ".100ms"`.
2. **Symbol Extraction**: `FetchData.cpp` used `channel_name.find(".none.")` to extract the instrument name. When connecting to channels formatted as `book.<symbol>.100ms`, `find(".none.")` failed, resulting in an empty string unless `data.contains("instrument_name")` was matched.

### The Fix
1. Subscribed to `"book." + symbol + ".100ms"`:
   ```cpp
   if (symbol.rfind("book.", 0) == 0) {
       channels.push_back(symbol);
   } else {
       channels.push_back("book." + symbol + ".100ms");
   }
   ```
2. Extracted the symbol generically by reading the substring between `book.` and the next `.` delimiter, with fallback to `data["instrument_name"]`:
   ```cpp
   if (channel_name.rfind("book.", 0) == 0) {
       size_t start = 5; // length of "book."
       size_t end = channel_name.find('.', start);
       if (end != std::string::npos && end > start) {
           symbol = channel_name.substr(start, end - start);
       }
   }
   if (symbol.empty()) {
       symbol = data.value("instrument_name", "");
   }
   ```

---

## 3. Order Book Level Assignment vs Volume Accumulation Bug

### The Problem
1. In `OrderBook.hpp`:
   ```cpp
   void addBid(double price, double quantity) {
       bids[price] += quantity; // BUG: accumulates quantity rather than setting price level size
   }
   ```
   Deribit sends price-level aggregated order books (L2 public depth), where each update specifies the *total current volume* at that price level, not an incremental trade or single order. Adding `+= quantity` inflated order book levels whenever a level was refreshed.
2. In `TradingEngine.hpp`:
   ```cpp
   if (delta.action == BookAction::NEW) book.addBid(delta.price, delta.amount);
   else if (delta.action == BookAction::CHANGE) book.modifyBid(delta.price, delta.amount);
   else if (delta.action == BookAction::DELETE) book.cancelBid(delta.price);
   ```
   If `delta.action` was set to `BookAction::SNAPSHOT`, it matched none of the conditions, causing all snapshot deltas to be ignored.

### The Fix
1. In `OrderBook.hpp`:
   ```cpp
   void addBid(double price, double quantity) {
       if (!std::isfinite(price) || !std::isfinite(quantity) || price <= 0.0 || quantity <= 0.0) return;
       bids[price] = quantity;
   }
   void addAsk(double price, double quantity) {
       if (!std::isfinite(price) || !std::isfinite(quantity) || price <= 0.0 || quantity <= 0.0) return;
       asks[price] = quantity;
   }
   ```
2. In `TradingEngine.hpp`:
   ```cpp
   for (size_t i = 0; i < event.delta_count; ++i) {
       const auto& delta = event.deltas[i];
       if (delta.side == BookSide::BID) {
           if (delta.action == BookAction::DELETE || delta.amount <= 0.0) {
               book.cancelBid(delta.price);
           } else {
               book.modifyBid(delta.price, delta.amount);
           }
       } else {
           if (delta.action == BookAction::DELETE || delta.amount <= 0.0) {
               book.cancelAsk(delta.price);
           } else {
               book.modifyAsk(delta.price, delta.amount);
           }
       }
   }
   ```

---

## 4. Expired & Inactive Instruments in `config/symbols.json`

### The Problem
`data/fetch_symbol.py` was fetching instruments using:
```python
url = "https://test.deribit.com/api/v2/public/get_instruments?currency=BTC&kind=option&expired=false"
```
Deribit testnet retains recently settled options that have zero orders and expired dates. The script selected the first 10 instruments (`BTC-10OCT26-...`), which were already expired.

Consequently:
1. Deribit returned empty snapshots for those instruments (`bids=[]`, `asks=[]`).
2. Even if bids and asks had been present, `expiry_years` evaluated to `0.0`, triggering:
   ```cpp
   [PRICING WAIT] BTC-10OCT26-75000-C: contract is expired or expiry could not be parsed.
   ```

### The Fix
Updated `data/fetch_symbol.py` to query `public/get_book_summary_by_currency?currency=BTC&kind=option`, filter for active contracts with live `bid_price` and `ask_price`, and sort by volume:

```python
summary_url = f"https://test.deribit.com/api/v2/public/get_book_summary_by_currency?currency={CURRENCY}&kind={KIND}"
# Filter for contracts that have active bids and asks, sorted by volume
active = [
    item for item in data["result"]
    if item.get("bid_price") and item.get("ask_price")
]
active.sort(key=lambda x: x.get("volume", 0), reverse=True)
```
This populates `symbols.json` with active, liquid contracts with future expiries (e.g., `BTC-27NOV26-84000-C`, `BTC-14OCT26-86000-P`).

---

## 5. Chunking Support for Large Snapshot Batches

### The Problem
The SPSC event structure `L3BookUpdateEvent<64>` has a fixed capacity of 64 deltas. A full initial snapshot from Deribit for a deep book can contain more than 64 entries. Previously, entries beyond 64 were dropped because `addDelta()` returned `false`.

### The Fix
Added chunking in `FetchData.cpp`:
```cpp
auto push_event = [&]() {
    if (event.delta_count > 0 || event.is_snapshot) {
        while (!queue_.emplace(event) && is_running_) {
            #if defined(__x86_64__) || defined(_M_X64)
            _mm_pause();
            #endif
        }
        event.delta_count = 0;
        event.is_snapshot = false; // Only the first chunk clears the book
    }
};

if (!event.addDelta(act, side, price, amount)) {
    push_event();
    event.addDelta(act, side, price, amount);
}
```
If an update exceeds 64 levels, the current batch is pushed to the lock-free queue and parsing continues seamlessly without losing levels or clearing the book mid-stream.

---

## Verification

The system was rebuilt and executed via `./build/option_pricer`. The output confirms:
1. `BTC-PERPETUAL` ticker streams mark and last prices to `TradingEngine`.
2. Initial book snapshots are received and populated for all subscribed instruments.
3. Incremental delta updates (`change`) stream continuously.
4. Option mid-prices, implied volatility, Delta, Gamma, Vega, and spreads are computed in real time.
