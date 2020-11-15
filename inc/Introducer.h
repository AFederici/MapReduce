#ifndef INTRODUCER_H
#define INTRODUCER_H

#include <iostream> 
#include <string>
#include <vector>
#include <map> 
#include "Messages.h"
#include "Member.h"

using namespace std;

#define T_timeout 5
#define T_cleanup 5

#define INTRODUCER "172.22.94.78"
#define PORT "6000"

class Introducer {
public:
	map<pair<string, string>, pair<int, int>> membershipList;
	Member nodeInformation;

	Introducer();
	int heartbeatToNode(Member destination);
	int joinSystem(Member introdcuer);
	int listenForHeartbeat();
};

#endif //INTRODUCER_H