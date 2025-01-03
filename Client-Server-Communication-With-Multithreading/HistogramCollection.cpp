#include <iomanip>
#include <iostream>
#include <string.h>
#include "HistogramCollection.h"

using namespace std;


HistogramCollection::HistogramCollection () {
    hists = vector<Histogram*>();
}

HistogramCollection::~HistogramCollection () {
    for (auto hist : hists) {
        delete hist;
    }
    hists.clear();
}

void HistogramCollection::add (Histogram* hist) {
    hists.push_back(hist);
}

void HistogramCollection::update (int pno, double ts, double val) {
    hists[pno-1]->update(ts, val);
}

void HistogramCollection::print () {
    int nhists = hists.size();
    if (nhists <= 0) {
        cout << "Histogram collection is empty" << endl;
        return;
    }

    double* sum = new double[nhists];
    memset(sum, 0, nhists*sizeof(double));

    int nbins = hists[0]->size();
    vector<double> range = hists[0]->get_range();
    float delta = (range[1] - range[0]) / nbins;
    float st = range[0];

    int ndots = 15 + 6*nhists;
    for (int i = 0; i < ndots; i++) {
        cout << "-";
    }
    cout << endl;

    for (int i = 0; i < nbins; i++) {
        printf("[%5.2f,%5.2f): ", st, st + delta);
        for (int j = 0; j < nhists; j++) {
            printf("% 12.4f ", hists[j]->get_hist()[i]);
            sum[j] += hists[j]->get_hist()[i];
        }
        cout << endl;
        st += delta;
    }

    for (int i = 0; i < ndots; i++) {
        cout << "-";
    }
    cout << endl;

    printf("[%5.2f,%5.2f): ", range[0], range[1]);
    for (int j = 0; j < nhists; j++) {
        printf("% 12.4f ", sum [j]);
    }
    cout << endl;

    delete[] sum;
}
