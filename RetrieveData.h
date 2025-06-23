#pragma once

#include <string>
#include <vector>
#include <map>
#include "Stock.h"
#include <future>
#include <queue>
#include <condition_variable>
#include <thread>
#include <mutex>

using namespace std;
using namespace fre;

namespace fre{
    typedef map<string, map<string, double>> StockMap; 

    struct StockEarnings {
        string ticker;
        string date;
        double surprisePercent;

        StockEarnings(string t, string d, double s);
    };


    class DataRetriever {
    private:
        const string earningsFile;

    public:
        DataRetriever(const string& filename);

        void populateStockPrice( vector<StockEarnings>& stockPriceList, map<string, EarningsInfo>& earningsInfoMap);

        void sortAndDivideGroups(const vector<StockEarnings>& StockPrice, vector<StockEarnings>& Miss,
                                vector<StockEarnings>& Meet, vector<StockEarnings>& Beat);

        bool fetchHistoricalPrices(void* handle, const string& symbol, const string& start_date,const string& end_date,
                                    const string& api_token, StockMap& stockMap);

        vector<pair<int, double>> alignPriceToDay0(const string& announcementDate,const map<string, double>& priceHistory,int N);
    };


    map<int, double> convertAlignedVectorToMap(const vector<pair<int, double>>& vec);


    string ConvertDate_MMDDYYYY_to_YYYYMMDD(const string& date);

    map<string, map<int, double>> getAlignedBenchmarkPerStock(const vector<StockEarnings>& group,
        const map<string, double>& benchmarkPriceMap, int N, DataRetriever& retriever);
}