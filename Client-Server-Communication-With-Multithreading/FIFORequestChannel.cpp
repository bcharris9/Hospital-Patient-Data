#include "FIFORequestChannel.h"

using namespace std;

/*--------------------------------------------------------------------------*/
/*		CONSTRUCTOR/DESTRUCTOR FOR CLASS	R e q u e s t C h a n n e l		*/
/*--------------------------------------------------------------------------*/


/* Creates a "local copy" of the channel specified by the given name. 
	 If the channel does not exist, the associated IPC mechanisms are 
	 created. If the channel exists already, this object is associated with the channel.
	 The channel has two ends, which are conveniently called "SERVER_SIDE" and "CLIENT_SIDE".
	 If two processes connect through a channel, one has to connect on the server side 
	 and the other on the client side. Otherwise the results are unpredictable.

	 NOTE: If the creation of the request channel fails (typically happens when too many
	 request channels are being created) and error message is displayed, and the program
	 unceremoniously exits.

	 NOTE: It is easy to open too many request channels in parallel. Most systems
	 limit the number of open files per process.
	*/
FIFORequestChannel::FIFORequestChannel (const string _name, const Side _side) : my_name( _name), my_side(_side) {
	pipe1 = "fifo_" + my_name + "1";
	pipe2 = "fifo_" + my_name + "2";
		
	if (_side == SERVER_SIDE){
		wfd = open_pipe(pipe1, O_WRONLY);
		rfd = open_pipe(pipe2, O_RDONLY);
	}
	else{
		rfd = open_pipe(pipe1, O_RDONLY);
		wfd = open_pipe(pipe2, O_WRONLY);
		
	}
	
}

FIFORequestChannel::~FIFORequestChannel () { 
	close(wfd);
	close(rfd);

	remove(pipe1.c_str());
	remove(pipe2.c_str());
}

/*--------------------------------------------------------------------------*/
/*			MEMBER FUNCTIONS FOR CLASS	R e q u e s t C h a n n e l			*/
/*--------------------------------------------------------------------------*/

int FIFORequestChannel::open_pipe (string _pipe_name, int mode) {
	mkfifo (_pipe_name.c_str (), 0600);
	int fd = open(_pipe_name.c_str(), mode);
	if (fd < 0){
		EXITONERROR(_pipe_name);
	}
	return fd;
}

int FIFORequestChannel::cread (void* msgbuf, int msgsize) {
	return read (rfd, msgbuf, msgsize); 
}

int FIFORequestChannel::cwrite (void* msgbuf, int msgsize) {
	return write (wfd, msgbuf, msgsize);
}

string FIFORequestChannel::name () {
	return my_name;
}
