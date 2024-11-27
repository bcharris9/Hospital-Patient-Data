#include "Histogram.h"
#include <cmath>
using namespace std;


Histogram::Histogram (int _nbins, double _start, double _end) : nbins (_nbins), start(_start), end(_end) {
	hist = vector<double>(nbins, 0);
}

Histogram::~Histogram () {}

void Histogram::update (double ts, double value) {
	int bin_index = std::round(100*(ts - start) / (end - start) * nbins)/100;
	if (bin_index < 0) {
		bin_index= 0;
    }
	else if (bin_index >= nbins) {
		bin_index = nbins-1;
    }
    /*
    TODO:
        lock mutex of the Histogram object
        add ecg value to the Histogram bin indexed by bin_index
		add 1 to the Histogram bin indexed by bin_index (debug output)
        release mutex lock
    */
	std::unique_lock<std::mutex> lock(lck);
    hist[bin_index] += value;
}

int Histogram::size () {
	return nbins;		
}

vector<double> Histogram::get_range () {
	vector<double> r;
	r.push_back (start);
	r.push_back (end);
	return r;
}

const vector<double>& Histogram::get_hist () {
	return hist;
}
