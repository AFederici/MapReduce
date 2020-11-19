#include "../inc/Member.h"

Member::Member(string nodeIp, string nodePort, int nodeTimestamp, int nodeHeartbeatCounter)
{
	ip = nodeIp;
	port = nodePort;
	failed_flag = 0;
	timestamp = nodeTimestamp;
	heartbeatCounter = nodeHeartbeatCounter;
}

Member::Member(string nodeIp, string nodePort)
{
	ip = nodeIp;
	port = nodePort;
	timestamp = 0;
	failed_flag = 0;
	heartbeatCounter = 0;
}

Member::Member(string nodeIp, string nodePort, int nodeTimestamp)
{
	ip = nodeIp;
	port = nodePort;
	timestamp = nodeTimestamp;
	failed_flag = 0;
	heartbeatCounter = 0;
}

string Member::toString()
{
	string message = ip + "::" + port + "::" + to_string(timestamp);
	return message;
}

Member::Member(){
	ip = "";
	port = "";
	timestamp = 0;
	failed_flag = 0;
	heartbeatCounter = 0;
}
