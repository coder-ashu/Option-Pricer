#include "BlackScholes.hpp"
#include <cmath>
#include <algorithm>

namespace {
    constexpr double INV_SQRT_2PI = 0.3989422804014327; // 1.0 / std::sqrt(2 * M_PI)
}

double BlackScholes::norm_pdf(double x) {
    return INV_SQRT_2PI * std::exp(-0.5 * x * x);
}

double BlackScholes::norm_cdf(double x) {
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

double BlackScholes::calculateCallPrice(double S, double K, double T, double r, double v) {
    if (!std::isfinite(S) || !std::isfinite(K) || !std::isfinite(T) || !std::isfinite(r) ||
        !std::isfinite(v) || S <= 0.0 || K <= 0.0) return 0.0;
    if (T <= 0.0) return std::max(0.0, S - K);
    if (v <= 0.0) return std::max(0.0, S - K * std::exp(-r * T));

    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r + 0.5 * v * v) * T) / (v * sqrtT);
    double d2 = d1 - v * sqrtT;

    return S * norm_cdf(d1) - K * std::exp(-r * T) * norm_cdf(d2);
}

double BlackScholes::calculatePutPrice(double S, double K, double T, double r, double v) {
    if (!std::isfinite(S) || !std::isfinite(K) || !std::isfinite(T) || !std::isfinite(r) ||
        !std::isfinite(v) || S <= 0.0 || K <= 0.0) return 0.0;
    if (T <= 0.0) return std::max(0.0, K - S);
    if (v <= 0.0) return std::max(0.0, K * std::exp(-r * T) - S);

    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r + 0.5 * v * v) * T) / (v * sqrtT);
    double d2 = d1 - v * sqrtT;

    return K * std::exp(-r * T) * norm_cdf(-d2) - S * norm_cdf(-d1);
}

Greeks BlackScholes::calculateCallGreeks(double S, double K, double T, double r, double v) {
    Greeks g{};
    if (T <= 0.0 || v <= 0.0) {
        g.price = std::max(0.0, S - K);
        g.delta = (S > K) ? 1.0 : 0.0;
        g.gamma = 0.0;
        g.vega = 0.0;
        g.theta = 0.0;
        return g;
    }

    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r + 0.5 * v * v) * T) / (v * sqrtT);
    double d2 = d1 - v * sqrtT;

    double nd1 = norm_cdf(d1);
    double nd2 = norm_cdf(d2);
    double npd1 = norm_pdf(d1);
    double discount = std::exp(-r * T);

    g.price = S * nd1 - K * discount * nd2;
    g.delta = nd1;
    g.gamma = npd1 / (S * v * sqrtT);
    g.vega = S * npd1 * sqrtT * 0.01; // 1% vol change sensitivity
    g.theta = (-(S * npd1 * v) / (2.0 * sqrtT) - r * K * discount * nd2) / 365.0; // 1 day decay

    return g;
}

Greeks BlackScholes::calculatePutGreeks(double S, double K, double T, double r, double v) {
    Greeks g{};
    if (T <= 0.0 || v <= 0.0) {
        g.price = std::max(0.0, K - S);
        g.delta = (S < K) ? -1.0 : 0.0;
        g.gamma = 0.0;
        g.vega = 0.0;
        g.theta = 0.0;
        return g;
    }

    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r + 0.5 * v * v) * T) / (v * sqrtT);
    double d2 = d1 - v * sqrtT;

    double n_minus_d1 = norm_cdf(-d1);
    double n_minus_d2 = norm_cdf(-d2);
    double npd1 = norm_pdf(d1);
    double discount = std::exp(-r * T);

    g.price = K * discount * n_minus_d2 - S * n_minus_d1;
    // g.delta = nd1 - 1.0;
    g.delta = norm_cdf(d1) - 1.0;
    g.gamma = npd1 / (S * v * sqrtT);
    g.vega = S * npd1 * sqrtT * 0.01;
    g.theta = (-(S * npd1 * v) / (2.0 * sqrtT) + r * K * discount * n_minus_d2) / 365.0;

    return g;
}

namespace {
double solveImpliedVolatility(double marketPrice, double S, double K, double T, double r,
                              double initialVol, bool isCall, double tol, int maxIter) {
    if (!std::isfinite(marketPrice) || !std::isfinite(S) || !std::isfinite(K) ||
        !std::isfinite(T) || !std::isfinite(r) || S <= 0.0 || K <= 0.0 || T <= 0.0 ||
        marketPrice < 0.0) return 0.0;

    const double discount = std::exp(-r * T);
    const double lowerBound = isCall ? std::max(0.0, S - K * discount)
                                     : std::max(0.0, K * discount - S);
    const double upperBound = isCall ? S : K * discount;
    if (marketPrice < lowerBound - tol || marketPrice >= upperBound) return 0.0;
    if (marketPrice <= lowerBound + tol) return 0.0; // zero volatility at the bound

    auto priceAt = [&](double vol) {
        return isCall ? BlackScholes::calculateCallPrice(S, K, T, r, vol)
                      : BlackScholes::calculatePutPrice(S, K, T, r, vol);
    };

    double low = 1e-8;
    double high = std::clamp(std::isfinite(initialVol) ? initialVol : 0.5, 0.01, 5.0);
    while (priceAt(high) < marketPrice && high < 10.0) high = std::min(10.0, high * 2.0);
    if (priceAt(high) < marketPrice) return 0.0;

    for (int i = 0; i < std::max(1, maxIter); ++i) {
        const double mid = (low + high) * 0.5;
        const double modelPrice = priceAt(mid);
        if (std::abs(modelPrice - marketPrice) <= tol) return mid;
        if (modelPrice < marketPrice) low = mid;
        else high = mid;
    }
    return (low + high) * 0.5;
}
}

double BlackScholes::impliedVolatilityCall(double marketPrice, double S, double K, double T, double r,
                                           double initialVol, double tol, int maxIter) {
    return solveImpliedVolatility(marketPrice, S, K, T, r, initialVol, true, tol, maxIter);
}

double BlackScholes::impliedVolatilityPut(double marketPrice, double S, double K, double T, double r,
                                          double initialVol, double tol, int maxIter) {
    return solveImpliedVolatility(marketPrice, S, K, T, r, initialVol, false, tol, maxIter);
}
