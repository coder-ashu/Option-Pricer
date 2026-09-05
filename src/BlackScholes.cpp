#include "BlackScholes.hpp"
#include <cmath>
#include <algorithm>

// Calculate the Cumulative Normal Distribution
double BlackScholes::norm_cdf(double x) {
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

double BlackScholes::calculateCallPrice(double S, double K, double T, double r, double v) {
    // If the option is expiring exactly right now, it's just the intrinsic value
    if (T <= 0.0) return std::max(0.0, S - K);

    double d1 = (std::log(S / K) + (r + 0.5 * v * v) * T) / (v * std::sqrt(T));
    double d2 = d1 - v * std::sqrt(T);

    return S * norm_cdf(d1) - K * std::exp(-r * T) * norm_cdf(d2);
}

double BlackScholes::calculatePutPrice(double S, double K, double T, double r, double v) {
    if (T <= 0.0) return std::max(0.0, K - S);

    double d1 = (std::log(S / K) + (r + 0.5 * v * v) * T) / (v * std::sqrt(T));
    double d2 = d1 - v * std::sqrt(T);

    return K * std::exp(-r * T) * norm_cdf(-d2) - S * norm_cdf(-d1);
}