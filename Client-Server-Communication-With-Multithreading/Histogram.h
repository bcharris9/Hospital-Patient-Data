#ifndef _HISTOGRAM_H_
#define _HISTOGRAM_H_

#include <mutex>
#include <vector>



class Histogram {
private:
	std::vector<double> hist;
	int nbins;
	double start, end;

	std::mutex lck;

public:
    Histogram (int _nbins, double _start, double _end);
	~Histogram ();

	void update (double ts, double value);
    int size ();

	std::vector<double> get_range ();
    const std::vector<double>& get_hist ();
};

#endif
