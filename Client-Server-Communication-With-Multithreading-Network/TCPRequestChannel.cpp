#include "TCPRequestChannel.h"
#include <cstring>  
#include <unistd.h> 
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h>  
#include <netdb.h>
#include <cstdio>

using namespace std;

TCPRequestChannel::TCPRequestChannel (const std::string _ip_address, const int _port_no) {
    if (_ip_address.empty()) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(_port_no);
        server_addr.sin_addr.s_addr = INADDR_ANY;
        bind(sockfd, (sockaddr*) &server_addr, sizeof(server_addr));
        listen(sockfd, 128); 
    }
    else {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in client_addr;
        client_addr.sin_family = AF_INET;
        client_addr.sin_port = htons(_port_no);
        in_addr ip_addr_in;
        inet_pton(AF_INET, _ip_address.c_str(), &ip_addr_in);
        client_addr.sin_addr = ip_addr_in;
        connect(sockfd, (struct sockaddr*) &client_addr, sizeof(client_addr));
    }
}

TCPRequestChannel::TCPRequestChannel (int _sockfd) {
    sockfd = _sockfd;
}

TCPRequestChannel::~TCPRequestChannel () {
    close(sockfd);
}

int TCPRequestChannel::accept_conn () {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    return accept(sockfd, (sockaddr*)&client_addr, &client_len);
}

int TCPRequestChannel::cread (void* msgbuf, int msgsize) {
    return recv(sockfd, msgbuf, msgsize, 0);
}

int TCPRequestChannel::cwrite (void* msgbuf, int msgsize) {
    return send(sockfd, msgbuf, msgsize, 0);
}