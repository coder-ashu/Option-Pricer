#ifndef BLACK_SCHOLES_HPP
#define BLACK_SCHOLES_HPP

class BlackScholes {
public:
    // Calculates the theoretical price of a European Call Option
    // S = Current Stock Price (We will feed this from our OrderBook)
    // K = Strike Price of the Option
    // T = Time to Expiry (in Years. e.g., 30 days = 30.0 / 365.0)
    // r = Risk-Free Interest Rate (e.g., 0.05 for 5%)
    // v = Implied Volatility (Sigma. e.g., 0.20 for 20%)
    static double calculateCallPrice(double S, double K, double T, double r, double v);

    // Calculates the theoretical price of a European Put Option
    static double calculatePutPrice(double S, double K, double T, double r, double v);

private:
    // Standard Normal Cumulative Distribution Function (CDF)
    // Black-Scholes requires N(d1) and N(d2). This function computes that.
    static double norm_cdf(double x);
};

#endif // BLACK_SCHOLES_HPP