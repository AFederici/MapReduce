#ifndef NODE_H
#define NODE_H

#include <iostream> 
#include <string>
#include <vector>
#include <map>
#include <pthread.h>
#include <time.h>
#include <signal.h>

#include "Messages.h"
#include "Modes.h"
#include "Member.h"
#include "UdpSocket.h"
#include "TcpSocket.h"
#include "Logger.h"
#include "HashRing.h"

using namespace std;

#define INTRODUCER "172.22.94.78" // VM1
//#define INTRODUCER "172.22.158.81" // VM9
#define PORT "6000"

//#define LOG_VERBOSE 1

#define LOGGING_FILE_NAME "logs.txt"

// --- parameters (stay tuned) ---
#define T_period 300000 // in microseconds
#define T_timeout 15 // in T_period
#define T_cleanup 15 // in T_period
#define N_b 5 // how many nodes GOSSIP want to use

#define T_election 15 // in T_period
// ------

#define T_switch 3 // in seconds

void *runUdpServer(void *udpSocket);
void *runTcpServer(void *tcpSocket);
void *runTcpSender(void *tcpSocket);
void *runSenderThread(void *node);

class Node {
public:
	// (ip_addr, port_num, timestamp at insertion) -> (hb_count, timestamp, fail_flag)
	map<tuple<string, string, string>, tuple<int, int, int>> membershipList;
	Member nodeInformation;
	UdpSocket *udpServent;
	TcpSocket *tcpServent;
	int localTimestamp;
	int heartbeatCounter;
	time_t startTimestamp;
	string joinTimestamp;
	// unsigned long byteSent;
	// unsigned long byteReceived;

	
	ModeType runningMode;
	Logger* logWriter;
	bool activeRunning;
	bool prepareToSwitch;

	bool isLeader;
	bool isBlackout;
	int leaderPosition; // -1 for no leader
	int hashRingPosition;
	int proposedTime;
	int electedTime;
	string possibleSuccessorIP;
	string leaderIP;
	string leaderPort;
	HashRing *hashRing;

	map<string, vector<int>> fileList; //vector of node positions on a hashring for a given file where we can find that file stored
	map<string, string> localFilelist; // Map sdfsfilename -> localfilename
	map<string, tuple<int, int, int>> pendingRequests;
	map<string, tuple<string, string, string>> pendingSenderRequests;
	map<string, tuple<bool, bool, bool>> pendingRequestSent;

	
	Node();
	Node(ModeType mode);
	int getPositionOnHashring();
	int heartbeatToNode();
	int joinSystem(Member introdcuer);
	int listenToHeartbeats();
	int failureDetection();
	void updateNodeHeartbeatAndTime();
	void computeAndPrintBW(double diff);
	int requestSwitchingMode();
	int SwitchMyMode();
	void debugMembershipList();
	void startActive();
	// Added below Since MP2
	bool checkHashNodeCollision(int checkPosition);
	bool checkLeaderExist();
	bool findWillBeLeader();
	void proposeToBeLeader();
	void processElection(Messages messages);
	void processTcpMessages();
	void processRegMessages();
	void restartElection();
	void setUpLeader(string message, bool pending);
	string encapsulateFileList();
	void decapsulateFileList(string payload);
	string encapsulateMessage(map<PayloadType,string> payloads);
	string decapsulateMessage(string payload);
	bool isInVector(vector<int> v, int i);
	int updateHashRing();
	// leader related functions here
	void leaderCreateHashRing();
	int putFileSender(string filename, string sdfsfilename);
	int putFileMaster(string sdfsfilename);
	int putFileReeiver(string sdfsfilename);
	void listLocalFiles();
	void findNodesWithFile(string sdfsfilename);
	void debugSDFSFileList();
	void listSDFSFileList(string sdfsfilename);
	string populateSDFSFileList(MessageType type, string mem_list_to_send);
	void updateFileList(string sdfsfilename, int nodePosition);
	void checkFileListConsistency();
	
private:
	string populateMembershipMessage();
	string populateIntroducerMembershipMessage();
	void readMessage(string message);
	void processHeartbeat(string message);
	vector<string> splitString(string s, string delimiter);
	vector<tuple<string,string, string>> getRandomNodesToGossipTo();
	int hashingId(Member nodeMember, string joinTime);
};

#endif //NODE_H