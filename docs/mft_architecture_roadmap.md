# Medium-Frequency Trading (MFT) Options Pricing Architecture Roadmap

## 1. Executive Summary

This document outlines the architecture, mathematical enhancements, and high-performance engineering patterns required to transition the Vortex options engine from an analytical Black-Scholes toy model into an MFT (Medium-Frequency Trading) options pricing and automated quoting system.

MFT operating latencies typically sit between 100 microseconds and several milliseconds. At this frequency, pricing models must account for realistic market microstructure, volatility skew/surface dynamics, order-book queue imbalance, and Greek risk management without incurring excessive computational overhead.

---

## 2. Quantitative Modeling Upgrades

### 2.1 Beyond Flat Black-Scholes
The current implementation relies on flat Black-Scholes with constant volatility $\sigma$, assuming log-normal asset returns and continuous trading without jumps. In live options markets, this causes mispricing due to volatility skew/smile and discrete tail events.

- **Parametric Volatility Surface (SVI - Stochastic Volatility Inspired):**
  - Parameterize implied volatility across strikes and maturities using Gatheral's SVI formulation.
  - Represents the total implied variance $w(k) = \sigma_{BS}^2 T$ as a function of log-moneyness $k = \ln(K / F)$:
    $$w(k) = a + b \left( \rho (k - m) + \sqrt{(k - m)^2 + \sigma^2} \right)$$
  - Guarantees absence of butterfly and calendar spread arbitrage when parameterized with proper bounds.
- **SABR Model (Stochastic Alpha, Beta, Rho):**
  - Hagan's asymptotic formula provides an analytical approximation for the implied volatility smile across strikes for fixed expiries.
  - High computational efficiency suitable for fast recalibration cycles.
- **American Options Pricing:**
  - For exchange-traded American options (e.g., US equity options), early exercise premium is critical.
  - Recommended analytical approximations:
    - **Bjerksund-Stensland (2002)**: Faster and more stable than tree models, ideal for MFT hot paths.
    - **Barone-Adesi & Whaley (1987)**: Quick quadratic approximation for dividend-paying assets.

### 2.2 Greeks and Sensitivity Updates
Recalculating full pricing functions on every underlying tick is computationally wasteful.

- **Analytical First- and Second-Order Greeks:**
  - **Delta ($\Delta$):** $\frac{\partial P}{\partial S}$
  - **Gamma ($\Gamma$):** $\frac{\partial^2 P}{\partial S^2}$
  - **Vega ($\nu$):** $\frac{\partial P}{\partial \sigma}$
  - **Theta ($\Theta$):** $\frac{\partial P}{\partial t}$
  - **Vanna / Volga:** Cross-sensitivities ($\frac{\partial \Delta}{\partial \sigma}$, $\frac{\partial \text{Vega}}{\partial \sigma}$) for skew management.
- **Delta-Gamma Taylor Expansion for Real-Time Pricing:**
  - When the underlying spot moves by $\Delta S$ over short intervals:
    $$\Delta P \approx \Delta \cdot \Delta S + \frac{1}{2} \Gamma (\Delta S)^2 + \nu \cdot \Delta \sigma + \Theta \cdot \Delta t$$
  - This reduces the tick-to-quote evaluation latency from non-linear transcendentals ($e^x$, $\ln$, CDF) to basic polynomial arithmetic.

---

## 3. Market Microstructure & Signal Generation

### 3.1 Advanced Fair Value (Micro-Price)
Relying strictly on simple mid-price `(best_bid + best_ask) / 2` introduces adverse selection when queues are imbalanced.

- **Volume-Weighted Mid-Price (VWAP):**
  - Calculate mid-price weighted by top-of-book quantities:
    $$P_{\text{micro}} = \frac{Q_{\text{bid}} \cdot P_{\text{ask}} + Q_{\text{ask}} \cdot P_{\text{bid}}}{Q_{\text{bid}} + Q_{\text{ask}}}$$
- **Order Flow Imbalance (OFI):**
  - Track changes in queue sizes at the top $N$ levels to anticipate short-term spot directional pressure before updating option quotes.

### 3.2 Dynamic Spread & Inventory Skewing
- **Avellaneda-Stoikov Inventory Model:**
  - Adjust option bid/ask quotes based on cumulative inventory risk:
    $$r(s, q, t) = s - q \gamma \sigma^2 (T - t)$$
  - Shifts theoretical mid-quote downward when long inventory and upward when short inventory to encourage offsetting fills.

---

## 4. System Architecture: Two-Speed Engine Pattern

To maintain predictable sub-millisecond execution, the engine is partitioned into two asynchronous processing rings:

