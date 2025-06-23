#pragma once
#include <vector>
#include <string>

using namespace std;

namespace fre {
    // Draw CAAR curves using gnuplot
    void plotCAARCurves(const vector<double>& miss, const vector<double>& meet, const vector<double>& beat, int N);
}
