#include "GNUPlot.h"
#include <iostream>
#include <fstream>
#include <cstdio>      // for remove()
#include <cstdlib>     // for popen()
#include <string>

using namespace std;

namespace fre {
    void plotCAARCurves(const vector<double>& miss, const vector<double>& meet, const vector<double>& beat, int N) {
        const char* file1 = "MissGroup_CAAR.dat";
        const char* file2 = "MeetGroup_CAAR.dat";
        const char* file3 = "BeatGroup_CAAR.dat";

        FILE* f1 = fopen(file1, "w");
        FILE* f2 = fopen(file2, "w");
        FILE* f3 = fopen(file3, "w");

        for (int i = 0; i < 2 * N + 1; ++i) {
            int day = i - N;
            fprintf(f1, "%d %lf\n", day, miss[i]);
            fprintf(f2, "%d %lf\n", day, meet[i]);
            fprintf(f3, "%d %lf\n", day, beat[i]);
        }

        fclose(f1);
        fclose(f2);
        fclose(f3);

        FILE* gnuplotPipe = popen("gnuplot -persist", "w");
        if (gnuplotPipe) {
            fprintf(gnuplotPipe, "set title 'CAAR Curve for Event Study'\n");
            fprintf(gnuplotPipe, "set xlabel 'Days Around Event'\n");
            fprintf(gnuplotPipe, "set ylabel 'CAAR'\n");
            fprintf(gnuplotPipe, "set grid\n");
            fprintf(gnuplotPipe, "set key left top\n");
            fprintf(gnuplotPipe, "plot '%s' with lines title 'Miss', '%s' with lines title 'Meet', '%s' with lines title 'Beat'\n", file1, file2, file3);
            fflush(gnuplotPipe);
            printf("Press Enter to exit plot...\n");
            getchar();
            fprintf(gnuplotPipe, "exit\n");
            pclose(gnuplotPipe);
        } else {
            cerr << "Gnuplot not found." << endl;
        }

        remove(file1);
        remove(file2);
        remove(file3);
    }
}
