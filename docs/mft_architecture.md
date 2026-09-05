# Medium-Frequency Trading (MFT) Architecture Specification

## 1. Overview & Objective

This document outlines the architectural roadmap for transitioning the Vortex baseline engine into an options/equities Medium-Frequency Trading (MFT) platform.

### Target Characteristics
- **Latency & Holding Horizon**: Milliseconds to seconds (typically 5ms – 5000ms holding periods and re-quote latencies).
- **Core Focus**:
  - Microstructure alpha signals (Order Book Imbalance, Micro-price adjustments, and Order Flow Imbalance).
  - High-throughput option theoretical pricing with real-time analytical Greeks ($\Delta, \Gamma, \mathcal{V}, \Theta$).
  - Inventory-aware market making with asymmetric quoting (Avellaneda-Stoikov style skewing).
  - Pre-trade risk controls (position bounds, order rate throttling, and kill switches).

---

## 2. Target End-to-End Pipeline

