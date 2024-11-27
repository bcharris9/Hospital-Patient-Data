#include <fstream>
#include <chrono>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/wait.h>
#include <condition_variable>
#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "FIFORequestChannel.h"
// ecgno to use for datamsgs
#define EGCNO 1

using namespace std;
std::mutex outfile_mutex;

void patient_thread_function(BoundedBuffer &request_buffer, int patient_id, int n) {
    for (int i = 0; i < n; i++) {
        //datamessage for patient
        datamsg d(patient_id, i * 0.004, EGCNO);
        //push the datamessage to request buffet
        request_buffer.push((char*)&d, sizeof(d)); 
    }
    return;
}

void file_thread_function(string &filename, BoundedBuffer &request_buffer, FIFORequestChannel *&chan, int buffersize) {
    //Offset 0, length 0 to request file size
    filemsg fmsg(0, 0); 
    int filename_size = filename.size() + 1;
    int request_size = sizeof(filemsg) + filename_size;
    char *request = new char[request_size];
    //Copy filemsg and filename into the request buffer
    memcpy(request, &fmsg, sizeof(filemsg));
    strcpy(request + sizeof(filemsg), filename.c_str());
    //Send the request for file size
    chan->cwrite(request, request_size);
    //Receive the file size (server will respond with a long)
    __int64_t file_size;
    chan->cread(&file_size, sizeof(file_size));
    //Split the file into chunks and push requests onto the request buffer
    __int64_t offset = 0;
    while (offset < file_size) {
        int chunk_size = min(buffersize, (int)(file_size - offset));
        filemsg fmsg_chunk(offset, chunk_size);
        memcpy(request, &fmsg_chunk, sizeof(filemsg));
        strcpy(request + sizeof(filemsg), filename.c_str());
        //Push request for this chunk onto request buffer
        request_buffer.push(request, request_size);
        offset += chunk_size;
    }
    delete[] request;
}

void worker_thread_function(BoundedBuffer &request_buffer, BoundedBuffer &response_buffer, FIFORequestChannel *&worker_channel, int buffersize, string &filename, ofstream &outfile) {
    if (filename == "received/") {
        while(true) {   
            //request buffer
            char request[MAX_MESSAGE];
            //pop from the request buffer
            int bytes_received = request_buffer.pop(request, buffersize);
            //termination condition
            auto msg_type = (MESSAGE_TYPE*)request;
            if (*msg_type == QUIT_MSG) {
                //write quit message
                worker_channel->cwrite((char*)msg_type, sizeof(MESSAGE_TYPE));
                //delete channel
                delete worker_channel;
                worker_channel = nullptr;
                //break loop
                break;
            }
            //write the request to the server
            worker_channel->cwrite(request, bytes_received);
            //response buffer
            char response[MAX_MESSAGE];
            //read the response
            worker_channel->cread(response, sizeof(response)); 
            //update request
            ((datamsg*)request)->ecgno = *(double*)response;
            //push the updated "request" onto response buffer
            response_buffer.push((char*)&request, sizeof(datamsg));
        }
    }
    else {
        while (true) {
        //request buffer
        char request[MAX_MESSAGE];
        //pop from the request buffer
        int bytes_received = request_buffer.pop(request, buffersize);
        //termination condition
        auto msg_type = (MESSAGE_TYPE*)request;
        if (*msg_type == QUIT_MSG) {
            //write quit message
            worker_channel->cwrite((char*)msg_type, sizeof(MESSAGE_TYPE));
            //delete channel
            delete worker_channel;
            //break loop
            break;
        }
        //Send the file request to the server
        worker_channel->cwrite(request, bytes_received);
        //Prepare buffer for server response
        auto file_request = (filemsg*) request;
        int response_size = file_request->length;
        vector<char> response(response_size);
        worker_channel->cread(response.data(), response_size);
        //Write received data to the output file at the specified offset
        unique_lock<mutex> lock(outfile_mutex);  
        outfile.seekp(file_request->offset, ios::beg);
        outfile.write(response.data(), response_size);
        }
    }
    return;
}

void histogram_thread_function(BoundedBuffer &response_buffer, HistogramCollection &hc, datamsg quitter) {
    while (true) {
        //respone buffer
        char response[MAX_MESSAGE];
        //pop the response from the response buffer
        response_buffer.pop(response, sizeof(response));
        //tests if response is equal to the quit message
        if ((quitter.person == ((datamsg*)response)->person) && (quitter.seconds == ((datamsg*)response)->seconds) && (quitter.ecgno == ((datamsg*)response)->ecgno)){
            //break loop
            break;
        }
        //update histogram with values
        hc.update(((datamsg*)response)->person, ((datamsg*)response)->seconds, ((datamsg*)response)->ecgno);
    }
    return;
}

int main (int argc, char* argv[]) {
    int n = 1000;	// default number of requests per "patient"
    int p = 10;		// number of patients [1,15]
    int w = 50;	// default number of worker threads
	int h = 20;		// default number of histogram threads
    int b = 20;		// default capacity of the request buffer (should be changed)
	int m = MAX_MESSAGE;	// default capacity of the message buffer

	string f = "";	// name of file to be transferred
    // read arguments
    int opt;
	while ((opt = getopt(argc, argv, "n:p:w:h:b:m:f:")) != -1) {
		switch (opt) {
			case 'n':
				n = atoi(optarg);
                break;
			case 'p':
				p = atoi(optarg);
                break;
			case 'w':
				w = atoi(optarg);
                break;
			case 'h':
				h = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
                break;
			case 'm':
				m = atoi(optarg);
                break;
			case 'f':
				f = optarg;
                break;
		}
	}
    
	// fork and exec the server
    int pid = fork();
    if (pid == 0) {
        execl("./server", "./server", "-m", (char*) to_string(m).c_str(), nullptr);
    }
    
	// initialize overhead (including the control channel)
	FIFORequestChannel* chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
	HistogramCollection hc;
    MESSAGE_TYPE quit = QUIT_MSG;
    // making histograms and adding to collection
    for (int i = 0; i < p; i++) {
        Histogram* h = new Histogram(10, 0.0, 6.0);
        hc.add(h);
    }
	// record start time
    struct timeval start, end;
    gettimeofday(&start, 0);
    
    //open w number of channels
    FIFORequestChannel** worker_channels = new FIFORequestChannel*[w];
    //idk but it makes it work
    int largest=w;
    for (int i = 0; i < w; i++) {
        MESSAGE_TYPE new_channel_request = NEWCHANNEL_MSG;
        //MESSAGE_TYPE new_channel_request = NEWCHANNEL_MSG;
        chan->cwrite(&new_channel_request, sizeof(NEWCHANNEL_MSG));
        //name buffer
        char new_channel_name[MAX_MESSAGE];
        //read the new channel name
        chan->cread(new_channel_name, sizeof(new_channel_name)); // Read the new channel name from the server
        //Open the worker channel client side
        worker_channels[i] = new FIFORequestChannel(new_channel_name, FIFORequestChannel::CLIENT_SIDE);
    }
    ofstream outfile("received/temp.txt", ios::binary);
    string filename = "received/" + f;
    //histogram construction case
    if(f == "") {
        //create vectors for different thread types
        vector<thread> patient_threads, worker_threads, histogram_threads;
        //create quit messages for histogram
        datamsg quitter(-1, -1.0, -1.0);
        //create threads
        for (int i = 1; i <= p; i++) {patient_threads.push_back(thread(patient_thread_function, ref(request_buffer), i, n));}
        for (int i = 0; i < largest; i++) {worker_threads.push_back(thread(worker_thread_function, ref(request_buffer), ref(response_buffer), ref(worker_channels[i]), m, ref(filename), ref(outfile)));}
        for (int i = 0; i < h; i++) {histogram_threads.push_back(thread(histogram_thread_function, ref(response_buffer), ref(hc), quitter));}
        //join patient_threads
        for (auto &t : patient_threads) {t.join();}
        //clear patient_threads
        patient_threads.clear();
        //push quit message into request buffer
        for (int i = 0; i < largest; i++) {request_buffer.push((char*)&quit, sizeof(MESSAGE_TYPE));}
        //join worker_threads
        for (auto &t : worker_threads) {t.join();}
        //deallocate worker_channels
        delete[] worker_channels;
        worker_channels = nullptr;
        //clear worker threads
        worker_threads.clear();
        //push quit message into response buffer
        for (int i = 0; i < h; i++) {response_buffer.push((char*)&quitter, sizeof(datamsg));}
        //join histogram_threads
        for (auto &t : histogram_threads) {t.join();}
        //clear histogram_threads
        histogram_threads.clear();
        outfile.close();
        remove("received/temp.txt");
    }
    //file transfer case
    else if(f != "") {
        outfile.close();
        remove("received/temp.txt");
        ofstream outfile(filename, ios::binary);
        //create vectors for worker thread
        vector<thread> worker_threads;
        //create threads
        thread file_thread(file_thread_function, ref(f), ref(request_buffer), ref(chan), m);
        for (int i = 0; i < largest; i++) {worker_threads.push_back(thread(worker_thread_function, ref(request_buffer), ref(response_buffer), ref(worker_channels[i]), m, ref(filename), ref(outfile)));}    
        file_thread.join();
        //push quit message into request buffer
        for (int i = 0; i < largest; i++) {request_buffer.push((char*)&quit, sizeof(MESSAGE_TYPE));}
        //join worker_threads
        for (auto &t : worker_threads) {t.join();}
        //clear worker_threads
        worker_threads.clear();
        //deallocate worker_channels
        delete[] worker_channels;
        worker_channels = nullptr;
    }    
    

    // record end time
    gettimeofday(&end, 0);

    // print the results
	if (f == "") {
		hc.print();
	}
    int secs = ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) / ((int) 1e6);
    int usecs = (int) ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) % ((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

	// quit and close control channel
    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!" << endl;
    delete chan;

	// wait for server to exit
	wait(nullptr);
}













