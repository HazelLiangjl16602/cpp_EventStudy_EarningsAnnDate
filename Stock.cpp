// ======= Stock.cpp =======

#include "Stock.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include "RetrieveData.h"
#include <sstream>   // for istringstream, ostringstream
#include <iomanip>   // for get_time, put_time



using namespace std;

namespace fre {

    Stock::Stock() {}

    Stock::Stock(const StockMap& data, const map<string, EarningsInfo>& earnings)
        : stockData(data), earningsData(earnings) {}

    void Stock::addPriceData(const string& symbol, const map<string, double>& prices) {
        stockData[symbol] = prices;
    }

    void Stock::addEarningsInfo(const string& symbol, const EarningsInfo& info) {
        earningsData[symbol] = info;
    }

    void Stock::setAlignedPrices(const map<string, map<int, double>>& newAlignedPrices) {
        alignedPrices = newAlignedPrices;
    }

    // Fetch 2N+1 prices from alignedPrices
    vector<double> Stock::getDailyPrices(const string& symbol, int N) const {
        vector<double> prices;
        auto it = alignedPrices.find(symbol);
        if (it != alignedPrices.end()) {
            const auto& priceMap = it->second;
            for (int offset = -N; offset <= N; ++offset) {
                if (priceMap.count(offset)) {
                    prices.push_back(priceMap.at(offset));
                } else {
                    prices.push_back(std::nan(""));
                }
            }
        }
        return prices;
    }

    vector<double> Stock::getDailyReturns(const string& symbol, int N) const {
        vector<double> prices = getDailyPrices(symbol, N);
        vector<double> returns;
        for (size_t i = 1; i < prices.size(); ++i) {
            if (!std::isnan(prices[i-1]) && !std::isnan(prices[i]) && prices[i-1] > 0 && prices[i] > 0) {
                returns.push_back(log(prices[i] / prices[i-1]));
            } else {
                returns.push_back(std::nan(""));
            }
        }
        return returns;
    }

    vector<double> Stock::getCumulativeReturns(const string& symbol, int N) const {
        vector<double> returns = getDailyReturns(symbol, N);
        vector<double> cumulative(returns.size(), 0.0);
        if (!returns.empty()) {
            cumulative[0] = returns[0];
            for (size_t i = 1; i < returns.size(); ++i) {
                if (!std::isnan(cumulative[i-1]) && !std::isnan(returns[i]))
                    cumulative[i] = cumulative[i-1] + returns[i];
                else
                    cumulative[i] = std::nan("");
            }
        }
        return cumulative;
    }

    EarningsInfo Stock::getEarningsInfo(const string& symbol) const {
        auto it = earningsData.find(symbol);
        if (it != earningsData.end()) {
            return it->second;
        }
        return EarningsInfo();
    }

    void Stock::printStockSummary(const string& symbol) const {
        cout << "===== Stock Summary: " << symbol << " =====" << endl;
        auto earnIt = earningsData.find(symbol);
        if (earnIt != earningsData.end()) {
            const EarningsInfo& ei = earnIt->second;
            cout << "Announcement Date: " << ei.announcementDate << endl;
            cout << "Period Ending: " << ei.periodEnding << endl;
            cout << "Estimated EPS: " << ei.estimatedEPS << endl;
            cout << "Reported EPS: " << ei.reportedEPS << endl;
            cout << "Surprise: " << ei.surprise << endl;
            cout << "Surprise Percent: " << ei.surprisePercent << "%" << endl;
        } else {
            cout << "No earnings data available." << endl;
        }
    }

    // Compute log returns from aligned price map
    vector<double> Stock::computeLogReturns(const map<int, double>& alignedPrice, int N) {
        vector<double> logReturns;
        for (int day = -N; day <= N-1; ++day) {
            if (alignedPrice.count(day) && alignedPrice.count(day+1)) {
                double p1 = alignedPrice.at(day);
                double p2 = alignedPrice.at(day+1);
                if (p1 > 0 && p2 > 0) {
                    logReturns.push_back(log(p2/p1));
                } else {
                    logReturns.push_back(std::nan(""));
                }
            } else {
                logReturns.push_back(std::nan(""));
            }
        }
        return logReturns;
    }

    // Compute abnormal returns between stock and benchmark
    vector<double> Stock::computeAbnormalReturns(const vector<double>& stockLogReturns, const vector<double>& benchmarkLogReturns, int N) {
        vector<double> abnormalReturns;
        for (int i = 0; i < 2*N; ++i) {
            if (i < stockLogReturns.size() && i < benchmarkLogReturns.size()) {
                if (!std::isnan(stockLogReturns[i]) && !std::isnan(benchmarkLogReturns[i])) {
                    abnormalReturns.push_back(stockLogReturns[i] - benchmarkLogReturns[i]);
                } else {
                    abnormalReturns.push_back(std::nan(""));
                }
            } else {
                abnormalReturns.push_back(std::nan(""));
            }
        }
        return abnormalReturns;
    }
    
    map<int, double> Stock::getAlignedPriceForSymbol(const string& symbol) const {
        auto it = alignedPrices.find(symbol);
        if (it != alignedPrices.end()) return it->second;
        return {};
    }
    
    

    

    

} 