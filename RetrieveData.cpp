#include "RetrieveData.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstring>
#include "curl/curl.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cmath>
#include "Stock.h"
#include "GroupedStock.h"
using namespace fre;
using namespace std;

namespace fre {
    // Struct constructor
    StockEarnings::StockEarnings(string t, string d, double s)
        : ticker(t), date(d), surprisePercent(s) {}
        // used to store data for each stock's earnings


    // Constructor for DataRetriever
    DataRetriever::DataRetriever(const string& filename)
        : earningsFile(filename) {}


    // read csv file line by line and parses three groups + benchmark stock, read tht csv 
    void DataRetriever::populateStockPrice(
        vector<StockEarnings>& stockPriceList, // store ticker, date, and surprise --> used to sort and divided stocks into three groups 
        map<string, EarningsInfo>& earningsInfoMap) // map each ticker to detailed earnings data
    {
        ifstream fin(earningsFile); // open csv file set in the constructor 
        if (!fin) {
            cerr << "Error opening earnings announcement file." << endl;
            return;
        }

        string line;
        getline(fin, line); // skip header

        while (getline(fin, line)) {
            stringstream ss(line);
            string ticker, date, period, estimate, reported, surprise, surprise_pct;

            getline(ss, ticker, ',');
            getline(ss, date, ',');
            getline(ss, period, ',');
            getline(ss, estimate, ',');
            getline(ss, reported, ',');
            getline(ss, surprise, ',');
            getline(ss, surprise_pct, ','); // extract 7 fileds from csv row 

            try {
                if (ticker.empty()) {
                    cerr << "[Warning] Empty ticker in line: " << line << endl;
                    continue;
                }

                date = ConvertDate_MMDDYYYY_to_YYYYMMDD(date); // Standardizes the date format (e.g., from "12/15/2024" → "2024-12-15").
                
                // convert from string to double
                double est = stod(estimate);
                double rep = stod(reported);
                double surp = stod(surprise);
                double pct = stod(surprise_pct);

                // For stock price sorting
                stockPriceList.emplace_back(ticker, date, pct); // adds a StockEarnings(ticker, date, pct) to the list

                // fills out an EarningsInfo struct for the current ticker
                EarningsInfo info;
                info.ticker = ticker;
                info.announcementDate = date;
                info.periodEnding = period;
                info.estimatedEPS = est;
                info.reportedEPS = rep;
                info.surprise = surp;
                info.surprisePercent = pct;

                earningsInfoMap[ticker] = info; // stores that EarningsInfo in the map for quick access later

            } catch (...) {
                cerr << "[Warning] Invalid earnings data in line: " << line << endl;
            }
        }

        fin.close();
    }
    /* This function is foundational for Option1 and option2
    reads the full earnings announcement csv
    builts  vector<StockEarnings> → for sorting and assigning to Miss/Meet/Beat groups (based on surprise%).
            map<string, EarningsInfo> → for detailed reporting when the user queries a specific stock.
    feeds into next step for sort by surprise percentage
    */
    


    // comparator for sorting stocks by surprise% 
    // custom comparison function --> it compares two StockEarnings objects, return true if a has a smaller earnings surpise% than b
    // paased into sort() to sort stock list by surprise % in a ascending order
    bool compareSurprise(const StockEarnings& a, const StockEarnings& b) {
        return a.surprisePercent < b.surprisePercent;
    }
    

    // split stocks in to 3 equal groups --> prepares for separate analysis per group 
    // takes full list of stock earnings (StockPrice) from the csv
    // outputs three equal-sized group
    void DataRetriever::sortAndDivideGroups(const vector<StockEarnings>& StockPrice, vector<StockEarnings>& Miss,
                                            vector<StockEarnings>& Meet, vector<StockEarnings>& Beat) {
        vector<StockEarnings> sorted = StockPrice; // make a copy of the input list so we don't mutate the original, sorted this local sorted vector
        sort(sorted.begin(), sorted.end(), compareSurprise); // sorts all stocks in seconding order by surprise% --> bottom third is miss

        size_t size = sorted.size();
        size_t third = size / 3;

        Miss.assign(sorted.begin(), sorted.begin() + third);
        Meet.assign(sorted.begin() + third, sorted.begin() + 2 * third);
        Beat.assign(sorted.begin() + 2 * third, sorted.end()); 
    }
    /*  then use these three groups to:
    Fetch historical price data (per group)
    Compute abnormal returns (AR), AAR, CAAR
    Compare market reactions based on the group
    */

    // below are directly get from slide topic 10 for dynamically allocate and write fetch data into memory buffers
    struct MemoryStruct {
        char* memory;
        size_t size;
    };  // define a structure to hold a memory buffer and track its size  

    void* myrealloc(void* ptr, size_t size) {
        if (ptr)
            return realloc(ptr, size);
        else
            return malloc(size);
    }   // ptr is non null -> resize the existing buffer; if is null, it allocates a new one

    int write_data2(void* ptr, size_t size, size_t nmemb, void* data) {
        size_t realsize = size * nmemb;
        struct MemoryStruct* mem = (struct MemoryStruct*)data;  // callback function used by libcurl to store incoming data --> compute the actual data of the data block and casts data to a MemoryStruct*
        mem->memory = (char*)myrealloc(mem->memory, mem->size + realsize + 1);  // expand the memory buffer to fit the new data +1 null terminator
        if (mem->memory) {
            memcpy(&(mem->memory[mem->size]), ptr, realsize);
            mem->size += realsize;
            mem->memory[mem->size] = 0;
        }
        return realsize;    // if allocate succeeded --> copy the new data to the end of the existing buffer, update the size, null-terminate the string, return the amount of data written 
    }


    //fetches adjusted close prices from eodhistoricaldata using libcurl and the write_data2 callback
    // builds a URL, use libcurl to fetch data, stores parsed adjusted close prices into stockMap
    bool DataRetriever::fetchHistoricalPrices(void* handle, const string& symbol,
                                            const string& start_date,
                                            const string& end_date,
                                            const string& api_token,
                                            StockMap& stockMap) {
        struct MemoryStruct data;
        data.memory = NULL;
        data.size = 0;  // build a empty memory buffer to store the HTTP response
                                        
        string url_common = "https://eodhistoricaldata.com/api/eod/";
        string url_request = url_common + symbol + ".US?" +
                            "from=" + start_date +
                            "&to=" + end_date +
                            "&api_token=" + api_token +
                            "&period=d";


        // cout << "Fetching: " << symbol << endl;  // can be removed fecthing log -> I want to check which stock is being fetched
        static int counter = 0;
        if (++counter % 100 == 0) cout << counter << " stocks fetched..." << endl; // can be removed -> print every 100 stocks instead of every 1


        // libcurl setup 
        // sets the request URL, spoof a browser user-agent, disables SSL verification (optional but risky), registers custom write callback(write_data2) to receive data into MemoryStruct
        curl_easy_setopt(handle, CURLOPT_URL, url_request.c_str());
        curl_easy_setopt(handle, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data2);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void*)&data);

        CURLcode status = curl_easy_perform((CURL*)handle);
        if (status != CURLE_OK) {
            cerr << "curl_easy_perform() failed: " << curl_easy_strerror(status) << endl;
            return false;
        }   // executes the HTTP request

        stringstream sData;
        sData.str(data.memory);
        string line, sDate, sValue;
        // wraps the received data (data.memory) in a stringstream to parse line by line

        while (getline(sData, line)) {
            if (line.find('-') == string::npos) continue;   //Skip header or invalid lines

            sDate = line.substr(0, line.find_first_of(','));
            line.erase(line.find_last_of(','));
            sValue = line.substr(line.find_last_of(',') + 1);   // extract date and adjusted close price 

            try {
                double dPrice = stod(sValue);   // convert adjust close to double 
                stockMap[symbol][sDate] = dPrice; // populate stockMap keyed by: symbol → sDate → adjusted_close.
            } catch (...) {
                cerr << "Invalid price data: " << line << endl;
            }
        }

        free(data.memory);  // free dynamic memory buffer
        return true;
    }
    /*
    It retrieves historical adjusted close prices for each stock in the Miss/Meet/Beat groups.
    It builds a StockMap, a nested map: ticker → date → price.

    This data feeds into: Day 0 alignment (relative to earnings date) -> Log return calculation -> Abnormal return computation -> AAR / CAAR calculation
    This is called inside threads to improve performance during bulk fetching.
    */


    // convert dates format --> essential for consistency across APIs and internal data processing 
    string ConvertDate_MMDDYYYY_to_YYYYMMDD(const string& date) {
        int firstSlash = date.find('/');
        int secondSlash = date.find('/', firstSlash + 1);

        if (firstSlash == string::npos || secondSlash == string::npos) {
            cerr << "Invalid MM/DD/YYYY date format: " << date << endl;
            return "";
        }

        string mm = date.substr(0, firstSlash);
        string dd = date.substr(firstSlash + 1, secondSlash - firstSlash - 1);
        string yyyy = date.substr(secondSlash + 1);

        // Add leading zeros if needed
        if (mm.length() == 1) mm = "0" + mm;
        if (dd.length() == 1) dd = "0" + dd;

        return yyyy + "-" + mm + "-" + dd;
    }

// align tickers'price to day 0 
// aligns prices so that earnings day is day 0, and return (-N to +N) days of prices
// Step: 1. get and sort all trading dates from priceHistory
// Step: 2 search up to 7 days after announcementDate to find the closet trading day
// Step: 3 return a vector of {datOffset, price} from -N to +N
// returns nan("") for days where price is missing 
    vector<pair<int, double>> DataRetriever::alignPriceToDay0(
        const string& announcementDate,
        const map<string, double>& priceHistory,
        int N   // takes an announcement date, take that stock's full price history as a map (date -> adjusted price)
                // returns vector a each pair hold (offset_day, price) where offset_day = -N and +N
    ) {
        vector<pair<int, double>> alignedSeries;    // stores the final output: price data from -N and +N days relative to the announcement

        if (priceHistory.empty()) {
            cerr << "Price history is empty for announcement: " << announcementDate << endl;
            return alignedSeries;
        }   // input price history is missing, log a warning and return an empty result 

        // Step 1: sort trading dates 
        vector<string> sortedDates;
        for (const auto& it : priceHistory) {
            sortedDates.push_back(it.first);
        }
        sort(sortedDates.begin(), sortedDates.end());   // extract all date strings and sort them from earliest to latest - gives us an ordered timeline of trading days

        // Step 2: if there announcement date is not trading date, find the next closet date
        struct tm annDate = {};
        istringstream ss(announcementDate);
        ss >> get_time(&annDate, "%Y-%m-%d");
        if (ss.fail()) {
            cerr << "Parse announcement date failed: " << announcementDate << endl;
            return alignedSeries;   // parse the string announcementDate into a struct tm time struct --> allows for date arithmetic
        }  

        string day0Date = "";
        for (int shift = 0; shift <= 7; ++shift) {
            struct tm temp = annDate;
            temp.tm_mday += shift;
            mktime(&temp);
            ostringstream out;
            out << put_time(&temp, "%Y-%m-%d");
            string shiftedDate = out.str();

            if (binary_search(sortedDates.begin(), sortedDates.end(), shiftedDate)) {
                day0Date = shiftedDate;
                break;  // handles the common issue: the earnings date might not be a trading day
                        // checks up to 7 days forward for the next valid trading date
                        // as soon as it finds a matching trading day, it sets it as day0Date
            }
        }

        if (day0Date == "") {
            cerr << "Cannot align stock with announcement: " << announcementDate << endl;
            return alignedSeries;
        } 

        // Step 3: determine day 0's position in sortedDates
        auto itDay0 = find(sortedDates.begin(), sortedDates.end(), day0Date);
        if (itDay0 == sortedDates.end()) {
            cerr << "Unexpected error: day0 not found after binary search" << endl;
            return alignedSeries;
        }
        int day0Index = distance(sortedDates.begin(), itDay0);
        // find the index of day0Date in sortedDates, which will serve as the reference point (offset =0)

        // Step 4: retrieve -N to +N = 2N +1 
        for (int offset = -N; offset <= N; ++offset) {
            int targetIndex = day0Index + offset;
            if (targetIndex >= 0 && targetIndex < (int)sortedDates.size()) {
                string targetDate = sortedDates[targetIndex];
                double price = priceHistory.at(targetDate);
                alignedSeries.push_back({offset, price});
            } else {
                alignedSeries.push_back({offset, nan("")}); // 
            }                   // loop from -N to +N
                                // for each offset, calculate the corresponding index 
                                // if within range, extract the price and add to alignedSeries
                                // if out-of-range, insert a NaN value
        }

        return alignedSeries;   // return the full aligned price series 
    }
    /*
    Called once per stock after price data is fetched.
    Aligned prices feed into:
    Log return calculation: log(P_t / P_{t-1})
    Abnormal returns: AR_t = R_it - R_mt
    Bootstrapping AAR/CAAR
    */



    // turns a vector of Turns a vector of {offset, price} into a map<offset, price>. Used to simplify CAAR/AR calculation.
    map<int, double> convertAlignedVectorToMap(const vector<pair<int, double>>& vec) {
        map<int, double> result;
        for (const auto& p : vec) {
            result[p.first] = p.second;
        }
        return result;
    }

    // for each stock in a group: align benchmark price series to its earnings announcement, converts aligned price vectors to maps keyed by day offset
    map<string, map<int, double>> getAlignedBenchmarkPerStock(
        const vector<StockEarnings>& group,
        const map<string, double>& benchmarkPriceMap,
        int N,
        DataRetriever& retriever)   // takes a vector of stocks in a group, takes the entire pricehistory of the benchmark as map<string, double>
                                    // retriever is used to call alignPriceToDay0()
    {
        map<string, map<int, double>> result;   // create the return container. it's a map: key is ticker and value the map from relative day to IWV price

        for (const auto& stock : group) {       // loop over each stocl in the group (MissGroup, MeetGroup, or BeatGroup)
            vector<pair<int, double>> aligned =
                retriever.alignPriceToDay0(stock.date, benchmarkPriceMap, N);   // alignthe benchmark prices to that stock's earnings date using alignPricetoDay0
                                                                                // This returns a vector<pair<int, double>> with offset-day and benchmark price.
            result[stock.ticker] = convertAlignedVectorToMap(aligned);          // convert to a map
        }

        return result;
    }


}