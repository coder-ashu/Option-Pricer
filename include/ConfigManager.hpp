#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP

#include <string>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

struct PolygonConfig {
    std::string api_key;
    std::string host;
    std::string port;
    std::string endpoint;
    std::string target_symbol;
};

struct RiskConfig {
    double max_delta_threshold;
    int max_order_quantity;
    double daily_loss_limit_usd;
};

struct AppConfig {
    PolygonConfig polygon;
    RiskConfig risk;
    double initial_cash;

    static AppConfig loadFromFile(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open config file: " + filepath);
        }

        nlohmann::json j;
        file >> j;

        AppConfig cfg;
        cfg.polygon.api_key = j["polygon"]["api_key"].get<std::string>();
        cfg.polygon.host = j["polygon"]["host"].get<std::string>();
        cfg.polygon.port = j["polygon"]["port"].get<std::string>();
        cfg.polygon.endpoint = j["polygon"]["endpoint"].get<std::string>();
        cfg.polygon.target_symbol = j["polygon"]["target_symbol"].get<std::string>();

        cfg.risk.max_delta_threshold = j["risk_limits"]["max_delta_threshold"].get<double>();
        cfg.risk.max_order_quantity = j["risk_limits"]["max_order_quantity"].get<int>();
        cfg.risk.daily_loss_limit_usd = j["risk_limits"]["daily_loss_limit_usd"].get<double>();

        cfg.initial_cash = j["simulation"]["initial_cash"].get<double>();

        return cfg;
    }
};

#endif