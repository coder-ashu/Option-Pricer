#ifndef BLACK_SCHOLES_HPP
#define BLACK_SCHOLES_HPP

struct Greeks {
    double price;
    double delta;
    double gamma;
    double vega;
    double theta;
};

class BlackScholes {
public:
    static double calculateCallPrice(double S, double K, double T, double r, double v);
    static double calculatePutPrice(double S, double K, double T, double r, double v);

    // Single pass calculation of theoretical value and all core Greeks
    static Greeks calculateCallGreeks(double S, double K, double T, double r, double v);
    static Greeks calculatePutGreeks(double S, double K, double T, double r, double v);

    // Fast Newton-Raphson implied volatility solver
    static double impliedVolatilityCall(double marketPrice, double S, double K, double T, double r,
                                       double initialVol = 0.20, double tol = 1e-5, int maxIter = 100);

    static double norm_pdf(double x);
    static double norm_cdf(double x);
};

#endif // BLACK_SCHOLES_HPP
