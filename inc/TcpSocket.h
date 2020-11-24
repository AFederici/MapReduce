#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include <sys/wait.h>
#include <signal.h>

#include <iostream>
#include <string>
#include <map>
#include <utility>
#include <queue>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include "MessageTypes.h"
#include "Messages.h"
#include "Utils.h"
#include "FileObject.h"
#ifdef __linux__
#include <bits/stdc++.h>
#endif

using std::string;
using std::queue;
using std::to_string;
using std::map;
using std::make_tuple;
using std::get;

#define DEFAULT_TCP_BLKSIZE (128 * 1024)
#define BACKLOG 10
#define TCPPORT "4950"
class TcpSocket {
public:
	//tcp server directly handles PUTs. If put received, request from
	//receiving server for the data and store it, ends with adding PUTACK to
	//regMessages queue.. Other requests also put into one of the queueus.
	queue<string> qMessages; //election messages added to this queue
	queue<string> regMessages;//other messages added here
	queue<string> pendSendMessages;//keeps messages for the tcp client to send
	queue<string> mapleMessages; //keeps track of sending
	queue<string> mergeMessages; //keeps track of sending

	void bindServer(string port);
	void sendFile(string ip, string port, FILE * fp, int size);
	void putFile(string ip, string port, string localfilename, string sdfsfilename, string remoteLocalfilename);
	void putDirectory(string ip, string port); //put everything in tmp directory
	void sendLines(string ip, string port, string execFile, string localFile, string prefix, int start, int end);
	void sendMessage(string ip, string port, string message);
	int messageHandler(int sockfd, string payloadMessage, string returnID);
	int createConnection(string ip, string port);
	TcpSocket();
private:
	string getFileMetadata(int size, string checksum, string sdfsfilename, string localfilename, string remoteLocalfilename);
	string getDirMetadata();
};
#endif //TCPSOCKET_H
