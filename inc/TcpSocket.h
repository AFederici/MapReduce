#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include <iostream> 
#include <string>
#include <queue>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

// #include "Messages.h"
// #include "UdpSocket.h"

using namespace std;

#define DEFAULT_TCP_BLKSIZE (128 * 1024)
#define BACKLOG 10

#define TCPPORT "4950"

// void *runTcpServer(void *tcpSocket);
// void *runTcpClient(void *tcpSocket);

class TcpSocket {
public:
	queue<string> qMessages;
	queue<string> regMessages;
	queue<string> pendSendMessages;

	void bindServer(string port);
	void sendFile(string ip, string port, string localfilename, string sdfsfilename, string remoteLocalfilename);
	void sendMessage(string ip, string port, string message);
	TcpSocket();
private:
	string getFileMetadata(int size, string checksum, string sdfsfilename, string localfilename, string remoteLocalfilename);
	vector<string> splitString(string s, string delimiter);
};
#endif //TCPSOCKET_H