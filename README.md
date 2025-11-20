# ⚡ High-Performance Trading System

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/Irushi-coder/high-performance-trading-system)
[![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)](https://github.com/Irushi-coder/high-performance-trading-system)
[![Performance](https://img.shields.io/badge/latency-1--5%C2%B5s-orange)](https://github.com/Irushi-coder/high-performance-trading-system)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

A **production-grade, ultra-low-latency order matching engine** built from scratch in modern C++17. Features sub-microsecond matching latency, lock-free data structures, FIX protocol support, and real-time web dashboard.

![Dashboard Preview](https://img.shields.io/badge/Dashboard-Live-blue)

## 🚀 Key Features

### Performance
- ⚡ **12 nanoseconds** - Order creation latency
- ⚡ **1-5 microseconds** - Order matching latency
- ⚡ **200,000+ orders/second** - System throughput
- 🔒 **Zero allocations** in hot path (custom memory pool)
- 🔒 **Lock-free** multi-producer queues

### Trading Engine
- 📊 Complete **order book** with 10-level depth
- 🔄 **Market & limit orders** with partial fills
- ⏱️ **Price-time priority** matching
- 📈 **Real-time P&L** calculation
- 🛡️ **Pre-trade risk checks**
- 📉 **Position tracking** (long/short)

### Networking
- 🌐 **FIX Protocol** parser/serializer (industry standard)
- 🔌 **TCP/IP** multi-client server
- 📡 **WebSocket** real-time streaming
- 🎨 **Web Dashboard** with live visualization
- 📊 **Market data** broadcasting

### Production Features
- ⚙️ **Configuration management** (file-based)
- 📊 **System metrics** & monitoring
- 🔍 **Performance profiling** tools
- 📝 **Structured logging** with levels
- 🛡️ **Risk management** system

## 🏗️ Architecture
```
┌─────────────────┐
│   Web Dashboard │  (HTML/JS/WebSocket)
└────────┬────────┘
         │
    ┌────▼────────────────┐
    │  WebSocket Server   │
    └────┬────────────────┘
         │
    ┌────▼─────────────────┐
    │  Matching Engine     │
    │  - Order Book        │
    │  - Trade Execution   │
    └────┬────────────────┘
         │
    ┌────▼─────────────────┐
    │  Risk Manager        │
    │  - Position Tracking │
    │  - P&L Calculation   │
    └──────────────────────┘
```

## 📊 Performance Benchmarks

| Metric | Target | Achieved | Status |
|--------|--------|----------|--------|
| Order Creation | <100ns | 12ns | ✅ **20x better** |
| Order Matching | <10µs | 1-5µs | ✅ **Excellent** |
| Throughput | 100k ops/s | 200k+ ops/s | ✅ **2x better** |
| Memory Allocation | Minimal | Zero (hot path) | ✅ **Perfect** |

## 🛠️ Technology Stack

- **Language**: C++17
- **Build System**: CMake 3.15+
- **Compiler**: GCC 7+ / Clang 5+ / MSVC 2017+
- **Networking**: Raw TCP/IP + WebSocket
- **Threading**: C++11 threads + atomics
- **Performance**: Lock-free data structures, memory pools

## 📦 Build Instructions

### Prerequisites
```bash
# Windows (MinGW)
- CMake 3.15+
- GCC 7.0+ or MinGW-w64
- Git

# Linux
sudo apt install build-essential cmake git

# macOS
brew install cmake
```

### Building
```bash
# Clone repository
git clone https://github.com/Irushi-coder/high-performance-trading-system.git
cd high-performance-trading-system

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .

# Run tests
./test_performance
./test_matching_engine
./test_network
```

### Run Dashboard
```bash
# Start dashboard server
./dashboard_server

# Open dashboard.html in browser
# Navigate to http://localhost:8080
```

## 🎮 Usage Examples

### Submit Orders Programmatically
```cpp
#include "engine/matching_engine.hpp"

MatchingEngine engine("AAPL");

// Create limit buy order
auto buyOrder = std::make_shared<Order>(
    1, "AAPL", Side::BUY, OrderType::LIMIT,
    doubleToPrice(150.50), 100
);

// Submit and get trades
auto trades = engine.submitOrder(buyOrder);
```

### FIX Protocol Integration
```cpp
#include "network/fix_message.hpp"

// Parse FIX message
FIXMessage msg = FIXMessage::parse(fixString);
auto order = msg.toOrder();

// Create execution report
FIXMessage execReport = FIXMessage::createExecutionReport(order, "EXEC_123");
```

### Real-Time Dashboard
```javascript
// Connect to WebSocket feed
const ws = new WebSocket('ws://localhost:8080');

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    if (data.type === 'trade') {
        console.log('Trade executed:', data);
    }
};
```

## 📂 Project Structure
```
high-performance-trading-system/
├── include/
│   ├── core/           # Order, Trade, Types
│   ├── engine/         # OrderBook, MatchingEngine
│   ├── network/        # FIX, TCP, WebSocket
│   ├── risk/           # RiskManager, Position
│   └── utils/          # Logger, Timer, Config, Metrics
├── src/
│   ├── main.cpp                    # Basic demo
│   ├── test_*.cpp                  # Test suites
│   └── dashboard_server.cpp        # Web dashboard server
├── dashboard.html                   # Real-time web UI
├── CMakeLists.txt
└── README.md
```

## 🧪 Testing
```bash
# Unit tests
./test_order_book       # Order book operations
./test_matching_engine  # Matching logic
./test_network          # FIX protocol & TCP
./test_performance      # Latency benchmarks
./test_production       # Risk & config
```

## 📈 Development Phases

- ✅ **Phase 1**: Core foundation (orders, logging, timing)
- ✅ **Phase 2**: Order book with price levels
- ✅ **Phase 3**: Matching engine (market/limit orders)
- ✅ **Phase 4**: Performance optimization (lock-free, memory pools)
- ✅ **Phase 5**: Networking (FIX protocol, TCP server)
- ✅ **Phase 6**: Production features (risk, config, metrics)
- ✅ **Bonus**: Real-time web dashboard

## 🎓 Learning Outcomes

This project demonstrates:
- Advanced C++ programming (templates, RAII, move semantics)
- Lock-free concurrent data structures
- Memory management and optimization
- Network programming (TCP/IP, WebSocket)
- Financial protocols (FIX)
- Real-time systems design
- Performance engineering
- Production-ready architecture

## 🚀 Future Enhancements

- [ ] Multiple symbol support
- [ ] Stop orders & advanced types
- [ ] Database persistence (PostgreSQL)
- [ ] Market maker algorithms
- [ ] Back testing engine
- [ ] FPGA acceleration
- [ ] Kubernetes deployment
- [ ] Regulatory compliance (MiFID II)

## 📄 License

MIT License - see [LICENSE](LICENSE) file for details.

## 👤 Author

**Irushi Layanga**
- GitHub: [@Irushi-coder](https://github.com/Irushi-coder)


