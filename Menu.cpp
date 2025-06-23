#include "Menu.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <curl/curl.h>
#include "GNUPlot.h" 
#include <thread>
#include <mutex>
#include <cmath>
#include <algorithm>
#include <limits>  // Required for numeric_limits

using namespace std;
using namespace fre;

namespace fre{
    Menu::Menu() : choice(0), N(30), retriever("Russell3000EarningsAnnouncements.csv") {
        ProcessChoice();
    }

    void Menu::DisplayMenu() {
        cout << "\n===== Financial Event Study Menu =====\n";
        cout << "1. Enter N to retrieve historical data (N must be 30-60)\n";
        cout << "2. Pull information for one stock\n";
        cout << "3. Show AAR, AAR-STD, CAAR, CAAR-STD for one group\n";
        cout << "4. Show CAAR gnuplot graph for all groups\n";
        cout << "5. Exit\n";
        cout << "=======================================\n";
    }

    void Menu::ProcessChoice() {
        while (true) {
            DisplayMenu();
            cout << "Please enter your choice: ";
            cin >> choice;
    
            // ====== Input validation starts ======
            // If user input is invalid (e.g., letters or symbols), clear the error
            if (cin.fail()) {
                cin.clear(); // Clear error flag on cin
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Discard invalid input
                cout << "Invalid input. Please enter a number between 1 and 5.\n" << endl;
                continue; // Ask for input again
            }
            // ====== Input validation ends ======
    
            // Execute the corresponding menu option
            switch (choice) {
                case 1:
                    HandleOption1();
                    break;
                case 2:
                    HandleOption2();
                    break;
                case 3:
                    HandleOption3();
                    break;
                case 4:
                    HandleOption4();
                    break;
                case 5:
                    HandleOption5(); // Exit the menu
                    return;
                default:
                    // Catch out-of-range numeric input
                    cout << "Invalid choice. Please enter a number between 1 and 5.\n" << endl;
            }
        }
    }

    void Menu::HandleOption1() {
        cout << "Enter N (30 <= N <= 60): ";
        cin >> N;
        if (N < 30 || N > 60) {
            cout << "Invalid N! Must be between 30 and 60." << endl;
            return;
        }

        cout << "Fetching historical data for 2N+1 days with N = " << N << "..." << endl;

        // ====== Retrieve data from Earnings Anncouement ======
        vector<StockEarnings> StockPrice; // from RetrieveData ticker(t): date(d), surprisePercent(s)
        map<string, EarningsInfo> earningsInfoMap; // for info display from Stock: ticker, announcementData, periodEnding, estimatedEPS, reportedEPS, surprise, surprisePercent
        retriever.populateStockPrice(StockPrice, earningsInfoMap);  // parse EarningAnnco.CSV
                                                                    //create StockPrice & earningInfoMap --> later will be used in option2
    

        // ====== sort StockPrice vector by surproise% and divided into 3 groups ======
        retriever.sortAndDivideGroups(StockPrice, MissGroup, MeetGroup, BeatGroup);


        // ====== Retrieve data from EODHD for tickers and benchmark ======    
        curl_global_init(CURL_GLOBAL_ALL);
        // StockMap: from Stock typedef map<string, map<string, double>> StockMap: ticker -> date -> price
        StockMap MissStockMap, MeetStockMap, BeatStockMap;
        StockMap BenchmarkMap;
    
        string startDate = "2024-06-15";
        string endDate = "2025-05-07";
        string api_token = "680439755dd504.07501587";

        int counter = 0;  

        // ====== Use of threads ======
        mutex mapMutex;
        
        auto fetchWithLimit = [&](const vector<StockEarnings>& group, StockMap& targetMap) {
            size_t i = 0;
            const int maxThreads = 6;
            while (i < group.size()) {
                vector<thread> tempThreads;
                for (int t = 0; t < maxThreads && i < group.size(); ++t, ++i) {
                    tempThreads.emplace_back([&, i] {
                        const auto& stock = group[i];
                        StockMap tempMap;
                        CURL* threadHandle = curl_easy_init(); //per-thread handle
                        if (threadHandle && retriever.fetchHistoricalPrices(threadHandle, stock.ticker, startDate, endDate, api_token, tempMap)) {
                            lock_guard<mutex> lock(mapMutex);
                            targetMap[stock.ticker] = tempMap[stock.ticker];
                            counter++;
                        }
                        if (threadHandle) curl_easy_cleanup(threadHandle);
                    });
                }
                for (auto& th : tempThreads) {
                    if (th.joinable()) th.join();
                }
            }
        };
        fetchWithLimit(MissGroup, MissStockMap);
        fetchWithLimit(MeetGroup, MeetStockMap);
        fetchWithLimit(BeatGroup, BeatStockMap);
 
        CURL* bmHandle = curl_easy_init();
        retriever.fetchHistoricalPrices(bmHandle, "IWV", startDate, endDate, api_token, BenchmarkMap);

        curl_easy_cleanup(bmHandle);
        curl_global_cleanup();

        // ====== End of Fetching  ======    

        
        // ====== Data cleaning process - prepare for calculation and bootstrap   ======    
        
        for (const auto& stock : MissGroup) {
            MissStock.addSymbol(stock.ticker);
            MissStock.addEarningsInfo(stock.ticker, earningsInfoMap[stock.ticker]);
        }
        for (const auto& stock : MeetGroup) {
            MeetStock.addSymbol(stock.ticker);
            MeetStock.addEarningsInfo(stock.ticker, earningsInfoMap[stock.ticker]);
        }
        for (const auto& stock : BeatGroup) {
            BeatStock.addSymbol(stock.ticker);
            BeatStock.addEarningsInfo(stock.ticker, earningsInfoMap[stock.ticker]);
        }
        
        auto MissBM = getAlignedBenchmarkPerStock(MissGroup, BenchmarkMap["IWV"], N, retriever);
        auto MeetBM = getAlignedBenchmarkPerStock(MeetGroup, BenchmarkMap["IWV"], N, retriever);
        auto BeatBM = getAlignedBenchmarkPerStock(BeatGroup, BenchmarkMap["IWV"], N, retriever);

        map<string, map<int, double>> MissAlignedPrices, MeetAlignedPrices, BeatAlignedPrices;
        
        for (const auto& stock : MissGroup) {
        auto aligned = retriever.alignPriceToDay0(stock.date, MissStockMap[stock.ticker], N);
        MissAlignedPrices[stock.ticker] = convertAlignedVectorToMap(aligned);
        }
        MissStock.setAlignedPrices(MissAlignedPrices); 
        for (const auto& stock : MeetGroup) {
        auto aligned = retriever.alignPriceToDay0(stock.date, MeetStockMap[stock.ticker], N);
        MeetAlignedPrices[stock.ticker] = convertAlignedVectorToMap(aligned);
        }
        MeetStock.setAlignedPrices(MeetAlignedPrices);
        for (const auto& stock : BeatGroup) {
        auto aligned = retriever.alignPriceToDay0(stock.date, BeatStockMap[stock.ticker], N);
        BeatAlignedPrices[stock.ticker] = convertAlignedVectorToMap(aligned);
        }
        BeatStock.setAlignedPrices(BeatAlignedPrices);




      
        map<string, vector<double>> MissAR, MeetAR, BeatAR;

        // ==== Miss Group Calculation & Filtering ====
        for (const auto& [symbol, bmMap] : MissBM) {
            auto stockLog = MissStock.computeLogReturns(MissStock.getAlignedPriceForSymbol(symbol), N);
            auto bmLog = MissStock.computeLogReturns(bmMap, N);
            auto AR = MissStock.computeAbnormalReturns(stockLog, bmLog, N);
        
            int validCount = count_if(AR.begin(), AR.end(), [](double x) { return !std::isnan(x); });
            if (validCount < 2 * N) {
                cout << "[Miss Group] Skipped " << symbol << " due to insufficient valid AR data (" 
                     << validCount << " < " << 2 * N << ")" << endl;
                continue;
            }
        
            MissAR[symbol] = AR;
        }  
        // ==== Meet Group Calculation & Filtering ====
        for (const auto& [symbol, bmMap] : MeetBM) {
            auto stockLog = MeetStock.computeLogReturns(MeetStock.getAlignedPriceForSymbol(symbol), N);
            auto bmLog = MeetStock.computeLogReturns(bmMap, N);
            auto AR = MeetStock.computeAbnormalReturns(stockLog, bmLog, N);
        
            int validCount = count_if(AR.begin(), AR.end(), [](double x) { return !std::isnan(x); });
            if (validCount < 2 * N) {
                cout << "[Meet Group] Skipped " << symbol << " due to insufficient valid AR data (" 
                     << validCount << " < " << 2 * N << ")" << endl;
                continue;
            }
        
            MeetAR[symbol] = AR;
        }       
        // ==== Beat Group Calculation & Filtering ====
        for (const auto& [symbol, bmMap] : BeatBM) {
            auto stockLog = BeatStock.computeLogReturns(BeatStock.getAlignedPriceForSymbol(symbol), N);
            auto bmLog = BeatStock.computeLogReturns(bmMap, N);
            auto AR = BeatStock.computeAbnormalReturns(stockLog, bmLog, N);
        
            int validCount = count_if(AR.begin(), AR.end(), [](double x) { return !std::isnan(x); });
            if (validCount < 2 * N) {
                cout << "[Beat Group] Skipped " << symbol << " due to insufficient valid AR data (" 
                     << validCount << " < " << 2 * N << ")" << endl;
                continue;
            }
        
            BeatAR[symbol] = AR;
        }

        // ==== Prepare symbol list ====
        vector<string> MissSymbols, MeetSymbols, BeatSymbols;
        for (const auto& s : MissAR) MissSymbols.push_back(s.first);
        for (const auto& s : MeetAR) MeetSymbols.push_back(s.first);
        for (const auto& s : BeatAR) BeatSymbols.push_back(s.first);


        // ==== Bootstrapping and statistics calculation  ====
        bootstrapGroup(MissAR, MissSymbols, N, 40, Miss_AAR, Miss_CAAR);
        bootstrapGroup(MeetAR, MeetSymbols, N, 40, Meet_AAR, Meet_CAAR);
        bootstrapGroup(BeatAR, BeatSymbols, N, 40, Beat_AAR, Beat_CAAR);

         
        computeMeanAndStd(Miss_AAR, Miss_AAR_mean, Miss_AAR_std);
        computeMeanAndStd(Miss_CAAR, Miss_CAAR_mean, Miss_CAAR_std);
        computeMeanAndStd(Meet_AAR, Meet_AAR_mean, Meet_AAR_std);
        computeMeanAndStd(Meet_CAAR, Meet_CAAR_mean, Meet_CAAR_std);
        computeMeanAndStd(Beat_AAR, Beat_AAR_mean, Beat_AAR_std);
        computeMeanAndStd(Beat_CAAR, Beat_CAAR_mean, Beat_CAAR_std);



        // ==== Display results  ====
        cout << "============================================" << endl;
        cout << "Group#      AAR         AAR-STD    CAAR        CAAR-STD" << endl;
        cout << "--------------------------------------------------------" << endl;
        cout << fixed << setprecision(6);
        cout << "Miss      " << Miss_AAR_mean[N] << "   " << Miss_AAR_std[N] << "   " << Miss_CAAR_mean[N] << "   " << Miss_CAAR_std[N] << endl;
        cout << "Meet      " << Meet_AAR_mean[N] << "   " << Meet_AAR_std[N] << "   " << Meet_CAAR_mean[N] << "   " << Meet_CAAR_std[N] << endl;
        cout << "Beat      " << Beat_AAR_mean[N] << "   " << Beat_AAR_std[N] << "   " << Beat_CAAR_mean[N] << "   " << Beat_CAAR_std[N] << endl;
        cout << "============================================" << endl;
    }

    void Menu::HandleOption2() {
        string ticker;
        cout << "Enter the stock ticker you want to query: ";
        cin >> ticker;
    
        GroupedStock* targetGroup = nullptr;
    
        if (MissStock.hasSymbol(ticker)) {
            cout << "Stock " << ticker << " belongs to Miss Group." << endl;
            targetGroup = &MissStock;
        } else if (MeetStock.hasSymbol(ticker)) {
            cout << "Stock " << ticker << " belongs to Meet Group." << endl;
            targetGroup = &MeetStock;
        } else if (BeatStock.hasSymbol(ticker)) {
            cout << "Stock " << ticker << " belongs to Beat Group." << endl;
            targetGroup = &BeatStock;
        } else {
            cout << "Ticker not found." << endl;
            return;
        }
    
        
        cout << fixed << setprecision(6);
        const int colPerRow = 6;
        const int width = 20;
    
        
        vector<double> prices = targetGroup->getDailyPrices(ticker, N);
        vector<double> logReturns = targetGroup->getDailyReturns(ticker, N);
        vector<double> cumReturns = targetGroup->getCumulativeReturns(ticker, N);
    
        
        cout << "\n=== Daily Aligned Prices ===" << endl;
        for (int i = 0; i < prices.size(); ++i) {
            int day = i - N;
            cout << left << setw(width) << ("Day " + to_string(day) + ": " + to_string(prices[i]));
            if ((i + 1) % colPerRow == 0) cout << endl;
        }
        cout << endl;
    
        
        cout << "\n=== Daily Log Returns ===" << endl;
        for (int i = 0; i < logReturns.size(); ++i) {
            int day = i - N + 1;
            cout << left << setw(width) << ("Day " + to_string(day) + ": " + to_string(logReturns[i]));
            if ((i + 1) % colPerRow == 0) cout << endl;
        }
        cout << endl;
    
        
        cout << "\n=== Cumulative Log Returns ===" << endl;
        for (int i = 0; i < cumReturns.size(); ++i) {
            int day = i - N + 1;
            cout << left << setw(width) << ("Day " + to_string(day) + ": " + to_string(cumReturns[i]));
            if ((i + 1) % colPerRow == 0) cout << endl;
        }
        cout << endl;
    
        
        cout << "\n=== Earnings Info ===" << endl;
        targetGroup->printStockSummary(ticker);
    }  


    void Menu::HandleOption3() {
        cout << "Select group to view statistics: (1-Miss, 2-Meet, 3-Beat): ";
        int groupChoice;
        cin >> groupChoice;

        vector<double> aar_mean, aar_std, caar_mean, caar_std;
        string groupName;

        if (groupChoice == 1) {
            aar_mean = Miss_AAR_mean;
            aar_std = Miss_AAR_std;
            caar_mean = Miss_CAAR_mean;
            caar_std = Miss_CAAR_std;
            groupName = "Miss";
        } else if (groupChoice == 2) {
            aar_mean = Meet_AAR_mean;
            aar_std = Meet_AAR_std;
            caar_mean = Meet_CAAR_mean;
            caar_std = Meet_CAAR_std;
            groupName = "Meet";
        } else if (groupChoice == 3) {
            aar_mean = Beat_AAR_mean;
            aar_std = Beat_AAR_std;
            caar_mean = Beat_CAAR_mean;
            caar_std = Beat_CAAR_std;
            groupName = "Beat";
        } else {
            cout << "Invalid group selection." << endl;
            return;
        }

        int effectiveLength = 2 * N + 1; 

        cout << "\n=== Avg AARs for Group " << groupName << " ===\n";
        for (int i = 0; i < effectiveLength; ++i) {
            cout << fixed << setprecision(6) << aar_mean[i] << " ";
            if ((i + 1) % 10 == 0) cout << endl;
        }

        cout << "\n\n=== AAR STD for Group " << groupName << " ===\n";
        for (int i = 0; i < effectiveLength; ++i) {
            cout << fixed << setprecision(6) << aar_std[i] << " ";
            if ((i + 1) % 10 == 0) cout << endl;
        }

        cout << "\n\n=== Avg CAARs for Group " << groupName << " ===\n";
        for (int i = 0; i < effectiveLength; ++i) {
            cout << fixed << setprecision(6) << caar_mean[i] << " ";
            if ((i + 1) % 10 == 0) cout << endl;
        }

        cout << "\n\n=== CAAR STD for Group " << groupName << " ===\n";
        for (int i = 0; i < effectiveLength; ++i) {
            cout << fixed << setprecision(6) << caar_std[i] << " ";
            if ((i + 1) % 10 == 0) cout << endl;
        }

        cout << endl;
    }

    void Menu::HandleOption4() {
        cout << "Drawing CAAR curves for all groups using gnuplot...\n";
        plotCAARCurves(Miss_CAAR_mean, Meet_CAAR_mean, Beat_CAAR_mean, N);
    }

    void Menu::HandleOption5() {
        cout << "Drawing AAR curvs for all groups using gnuplot" << endl;
    }

    void Menu::HandleOption6() {
        cout << "Exiting program..." << endl;
    }
}