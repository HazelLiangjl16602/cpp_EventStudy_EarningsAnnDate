// ===== Menu.h =====
#pragma once
#include "RetrieveData.h"
#include "Stock.h"
#include "GroupedStock.h"
#include "BootStrap.h"
#include <vector>
#include <map>
#include <string>

using namespace std;
using namespace fre;
namespace fre{
    class Menu {
        private:
        int choice;
        int N;
        DataRetriever retriever;

        vector<StockEarnings> MissGroup, MeetGroup, BeatGroup;
        GroupedStock MissStock, MeetStock, BeatStock;

        vector<vector<double>> Miss_AAR, Miss_CAAR;
        vector<vector<double>> Meet_AAR, Meet_CAAR;
        vector<vector<double>> Beat_AAR, Beat_CAAR;

        vector<double> Miss_AAR_mean, Miss_AAR_std, Miss_CAAR_mean, Miss_CAAR_std;
        vector<double> Meet_AAR_mean, Meet_AAR_std, Meet_CAAR_mean, Meet_CAAR_std;
        vector<double> Beat_AAR_mean, Beat_AAR_std, Beat_CAAR_mean, Beat_CAAR_std;

    public:
        Menu();
        void DisplayMenu();
        void ProcessChoice();
        void HandleOption1();
        void HandleOption2();
        void HandleOption3();
        void HandleOption4();
        void HandleOption5();
        void HandleOption6();
    };
}