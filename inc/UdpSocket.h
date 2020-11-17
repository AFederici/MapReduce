#ifndef UDPSOCKET_H
#define UDPSOCKET_H
#include <iostream>
#include <string>
#include <queue>
#include <utility>
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
#ifdef __linux__
#include <bits/stdc++.h>
#endif

using std::string;
using std::queue;
using std::get;

#define MAXBUFLEN (128 * 1024)
#define LOSS_RATE 0

void *get_in_addr(struct sockaddr *sa);

// Add every message to a queue on receive, process this in the UDP thread
class UdpSocket {
public:
	unsigned long byteSent;
	unsigned long byteReceived;
	queue<string> qMessages;

	void bindServer(string port);
	void sendMessage(string ip, string port, string message);
	UdpSocket();

};
#endif //UDPSOCKET_H
