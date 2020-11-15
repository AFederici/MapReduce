#ifndef MEMBER_H
#define MEMBER_H

#include <iostream> 
#include <string>

using namespace std;

class Member {
public:
	string ip;
	string port;
	int timestamp;
	int heartbeatCounter;
	int failed_flag;
	Member();
	Member(string nodeIp, string nodePort, int nodeTimestamp, int heartbeatCounter);
	Member(string nodeIp, string nodePort);
	Member(string nodeIp, string nodePort, int nodeTimestamp);
	string toString();
};


#endif //MEMBER_H