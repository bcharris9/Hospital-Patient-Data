/*
	Original author of the starter code
	Tanzir Ahmed
	Department of Computer Science & Engineering
	Texas A&M University
	Date: 2/8/20

	Please include your Name, UIN, and the date below
	Name: Blake Harris
	UIN: 632004477
	Date: 09/09/2024
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include "common.h"
#include "FIFORequestChannel.h"

using namespace std;

/* Verifies if the commandline argument passed to client.cpp "num_of_servers"
   is one of the following numbers (2,4,8,16). */
void verify_server_count(int num_of_servers)
{
	//if num_of_servers allowed nothing happens
	//else returns error message
	if ((num_of_servers == 2) | (num_of_servers == 4) | (num_of_servers == 8) | (num_of_servers == 16)) {
	}
	else {
		cout << "There can only be 2, 4, 8 or 16 servers" << endl << "Exiting Now!" << endl;
	}
}

/* Establishes a new channel with the server by sending appropriate NEWCHANNEL_MSG message.
   Sets this new channel as main channel (point chan to new channel) for communication with the server. */
void set_new_channel(FIFORequestChannel *&chan, FIFORequestChannel **channels, int buffersize)
{
    //might be needed might not
	channels[0] = nullptr; 
	
    
	//Request a new channel
    MESSAGE_TYPE new_channel_request = NEWCHANNEL_MSG; // Assuming NEWCHANNEL_MSG is defined as part of protocol
    chan->cwrite(&new_channel_request, sizeof(MESSAGE_TYPE)); // Send request for a new channel

    char new_channel_name[256];
    chan->cread(new_channel_name, buffersize); // Read the new channel name from the server

    //Open the new channel on the client side
    FIFORequestChannel *new_channel = new FIFORequestChannel(new_channel_name, FIFORequestChannel::CLIENT_SIDE);

    //Set the new channel as the main channel
    channels[0] = new_channel;
	
}


/* Send datamsg request to server using "chan" FIFO Channel and
   cout the response with serverID */
void req_single_data_point(FIFORequestChannel *chan,
						   int person,
						   double time,
						   int ecg)
{
	//sending datamsg requesting specific ecg at a given time for a given patient
	//char buf[MAX_MESSAGE];
	datamsg dmsg(person, time, ecg);
	//memcpy(buf, &dmsg, sizeof(datamsg));
	chan->cwrite(&dmsg, sizeof(datamsg));
	
	//receiving response
	serverresponse resp(0,0.0);
	chan->cread(&resp, sizeof(serverresponse));
	
	//outputting ecg, serverid, and channel
	std::cout << resp.ecgData <<" data on server:"<< resp.serverId << " on channel " << chan->name() << endl << endl;

}


/* Sends 1000 datamsg requests to one server through FIFO Channel and
   dumps the data to x1.csv regardless of the patient ID */
void req_multiple_data_points(FIFORequestChannel *chan,
							  int person)
{
	//opens file x1.csv in recieved directory
	ofstream myfile("received/x1.csv");
	if (!myfile.is_open()) {
		std::cout << "Error: Could not open file x1.csv" << endl;
		return;
	}

	//set-up timing for collection
	timeval t;
	gettimeofday(&t, nullptr);
	double t1 = (1.0e6 * t.tv_sec) + t.tv_usec;

	//starting at t = 4 requests the next 1000 data points
	for (int i = 0; i < 1000; i++) {
		//increments current time by 4 ms
		double current_time = 4 + (i * 0.004);

		//creates at datamsg and serverrespone(reusable)
		datamsg dmsg1(person, current_time, 1);
		serverresponse resp(0,0.0);
		
		//requests ecg1 data and outputs it into x1.csv 
		chan->cwrite(&dmsg1, sizeof(datamsg));
		chan->cread(&resp, sizeof(serverresponse));
		myfile << current_time << "," << resp.ecgData << ",";
		
		//requests ecg2 data and outputs it into x1.csv 
		datamsg dmsg2(person, current_time, 2);
		chan->cwrite(&dmsg2, sizeof(datamsg));
		chan->cread(&resp, sizeof(serverresponse));
		myfile << resp.ecgData << endl;
		}


	//compute time taken to collect
	gettimeofday(&t, nullptr);
	double t2 = (1.0e6 * t.tv_sec) + t.tv_usec;

	//display time taken to collect
	cout << "Time taken to collect 1000 datapoints :: " << ((t2 - t1) / 1.0e6) << " seconds on channel " << chan->name() << endl;

	//closing file
	myfile.close();
}

/* Request Server to send contents of file (could be .csv or any other format) over FIFOChannel and
   dump it to a file in recieved folder */

void transfer_file(FIFORequestChannel *chan,
				   string filename,
				   int buffersize)
{
	
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

    cout << "File size: " << file_size << " bytes" << endl;

    //Open file in the "received/" directory to write the received data
    string output_file = "received/" + filename;
    ofstream ofs(output_file, ios::binary);

    // Set up timing
    timeval t;
	gettimeofday(&t, nullptr);
	double t1 = (1.0e6 * t.tv_sec) + t.tv_usec;

    //Request file in chunks
    int offset = 0;
    while (offset < file_size) {
        //Calculate the chunk size
        int chunk_size = min(buffersize, (int)(file_size - offset));

        //Create a filemsg for this chunk
        filemsg fmsg_chunk(offset, chunk_size);
        memcpy(request, &fmsg_chunk, sizeof(filemsg));
        strcpy(request + sizeof(filemsg), filename.c_str());

        //Send request for this chunk
        chan->cwrite(request, request_size);

        //Receive the chunk
        char *recv_buffer = new char[chunk_size];
        chan->cread(recv_buffer, chunk_size);

        //Write the chunk to the output file
        ofs.write(recv_buffer, chunk_size);

        //Clean up and move to the next chunk
        delete[] recv_buffer;
        offset += chunk_size;
    }

	//compute time taken to transfer
	gettimeofday(&t, nullptr);
	double t2 = (1.0e6 * t.tv_sec) + t.tv_usec;

	//display time taken to transfer
	cout << "Time taken to transfer file :: " << ((t2 - t1) / 1.0e6) << " seconds" << endl << endl;

	//closing file
	delete[] request;
	ofs.close();
}

int main(int argc, char *argv[])
{
	//initializes all possible variables
	int person = 0;
	double time = 0.0;
	int ecg = 0;

	int buffersize = MAX_MESSAGE;

	string filename = "";

	bool new_chan = false;

	int num_of_servers = 2;

	int opt;
	
	//uses getopt to collect command line arguments
	while ((opt = getopt(argc, argv, "p:t:e:m:f:s:c")) != -1)
	{
		switch (opt)
		{
		case 'p':
			person = atoi(optarg);
			break;
		case 't':
			time = atof(optarg);
			break;
		case 'e':
			ecg = atoi(optarg);
			break;
		case 'm':
			buffersize = atoi(optarg);
			break;
		case 'f':
			filename = optarg;
			break;
		case 's':
			num_of_servers = atoi(optarg);
			break;
		case 'c':
			new_chan = true;
			break;
		}
	}
	//makes sure number of servers is valid
	verify_server_count(num_of_servers);

	//bools help to decide which function to call
	bool data = ((person != 0) && (time != 0.0) && (ecg != 0));
	bool file = (filename != "");

	// fork to create servers
	for (int i = 1; i <= num_of_servers; i++) {
		if (fork()==0) {
			string buffer_size_str = to_string(buffersize);
			string server_id = to_string(i);
			const char* args[] = {"./server", "-m", (char*)buffer_size_str.c_str(), "-s", (char*)server_id.c_str(), NULL};
			execv(args[0], (char* const*)args);
			
			exit(0);
		}
	}

	//opens the requested channels clientside
	FIFORequestChannel** channels = new FIFORequestChannel*[num_of_servers+1];
	for (int i = 1; i <= num_of_servers; i++) {
        string channel_name = "control_" + std::to_string(i) + "_";
        channels[i] = new FIFORequestChannel(channel_name, FIFORequestChannel::CLIENT_SIDE);
    }

	// Initialize step and serverId
	float step = 16 / num_of_servers;
	int target_server = ceil(person / step);

	if (!filename.empty())
	{
		//creates a substring to try and detect patient ID
		string patient = filename.substr(0, filename.find('.'));
		std::istringstream ss(patient);
		int patient_id;
		
		//If patient ID is successfully extracted, determine which server should handle it
		if (ss >> patient_id) {
        	float step = 16.0 / num_of_servers;
       		target_server = ceil(patient_id / step);
        	//std::cout << "File transfer for patient " << patient_id << " will be handled by server " << target_server << std::endl;
		}
		//else, use the first server for file transfer
		else {
			target_server = 1;
		}
	}
	
	
	if (new_chan)
	{
		//call set_new_channel
		set_new_channel(channels[target_server], channels, buffersize);
		target_server = 0;
	}

	if (data)
	{
		//call req_single_data_point
		req_single_data_point(channels[target_server], person, time, ecg);
	}

	if (!data && person != 0)
	{
		//call req_multiple_data_points
		req_multiple_data_points(channels[target_server], person);
	}

	if (file)
	{
		//call transfer_file
		transfer_file(channels[target_server], filename, buffersize);

	}

MESSAGE_TYPE quit_msg = QUIT_MSG;
if (new_chan) {
        channels[0] -> cwrite(&quit_msg, sizeof(MESSAGE_TYPE));
        
        delete channels[0];
        channels[0] = nullptr;
    }

// Loop through all servers, sending a QUIT_MSG and deleting the channels
	for (int i = 1; i <= num_of_servers; i++) {
		if (channels[i]) {
			// Send the QUIT_MSG to each server
			channels[i]->cwrite(&quit_msg, sizeof(MESSAGE_TYPE));
			
			//Delete the client-side channel
			delete channels[i]; 
			channels[i] = nullptr;  // Set the pointer to nullptr to avoid dangling pointers
		}
	}

	// Finally, delete the array of channels
	delete[] channels;
	channels = nullptr;

	for (int i = 0; i <= num_of_servers; ++i) {
        int status;
        wait(&status);
    }

	return 0;
}