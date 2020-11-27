#ifndef NODE_H
#define NODE_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <set>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <fstream>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Messages.h"
#include "Modes.h"
#include "Member.h"
#include "UdpSocket.h"
#include "TcpSocket.h"
#include "Logger.h"
#include "HashRing.h"
#include "Utils.h"
#include "HashRing.h"

using namespace std;

#define INTRODUCER "fa20-cs425-g02-01.cs.illinois.edu" // VM1
#define PORT "6000"

#define LOGGING_FILE_NAME "logs.txt"
#define EXEC_CMD "./" //python for python script, ./ for an executable

// --- parameters (stay tuned) ---
#define T_period 300000 // in microseconds
#define T_timeout 20 // in T_period
#define T_cleanup 20 // in T_period
#define N_b 5 // how many nodes GOSSIP want to use
#define T_election 20 // in T_period
#define T_switch 3 // in seconds
#define T_maples 2 // lines to process, 2 for testing, probably 100 for real
//

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

	ModeType runningMode;
	Logger* logWriter;
	bool activeRunning;
	bool prepareToSwitch;

	bool isLeader;
	bool isBlackout; //true while DNS being handled
	int leaderPosition; // -1 for no leader
	int hashRingPosition;
	int proposedTime; //time when proposed to be leader with first ELECTION msg
	int electedTime; //keeps track of time of last ELECTIONACK
	string possibleSuccessorIP; //successor in hashRing
	string leaderIP;
	string leaderPort;
	HashRing *hashRing;

	map<string, vector<int>> fileList; //vector of node positions on a hashring for a given file where we can find that file stored
	map<string, string> localFilelist; // Map sdfsfilename -> localfilename
	map<string, tuple<int, int, int>> pendingRequests; //?
	map<string, tuple<string, string, string>> pendingSenderRequests; //?
	map<string, tuple<bool, bool, bool>> pendingRequestSent; //?

	//master properties for MAPLEJUICE
	map<string, vector<tuple<string, string>>> workerProcessing; //ip -> [ (file, chunk_start) ]

	//used for maple tasks because we want all or nothing results.
	//for juice once a key is processed and sent, it doesnt need to be redone on failure
	//however, this datastructure is used in the opposite way in juice. That is we build up
	//the set of what has been sent to master so we dont double send something.
	map<string, set<tuple<string,string>>> workerTasks;

	map<string, tuple<long int, int>> fileSizes; //used so master can partition in the map phase tuple is (bytes, lines)
	HashRing *workerRing; //hashring of worker nodes maple/juice
	map<string, vector<tuple<string, string>>> mapleSending; //originIP -> (file, chunk_start);
	vector<string> mapleKeys; //keys produced in maple phase
	queue<string> maplejuiceQ;
	string exe;
	string sdfsPre;
	string sdfsOut;
	bool maplejuiceClear;


	Node();
	Node(ModeType mode);
	int heartbeatToNode(); //send out memList to targets
	int joinSystem(Member introdcuer); //send memList with JOIN message to introducer
	int listenToHeartbeats(); //process UDP message queue
	int failureDetection(); //check for failures and update pendingRequests if sender failed. Also check for leader failure
	string updateNodeHeartbeatAndTime(); //update this nodes heartbeat and time in membershipList
	int requestSwitchingMode(); //send out SWREQ to switch modes
	int SwitchMyMode(); //actually switch running mode and print debug messages
	void startActive(); //initialize, get ready to join system
	bool checkHashNodeCollision(int checkPosition); //check if any member from memberlist hashes to checkPosition

	void tcpElectionProcessor(); //process TCP queue for election messages
	void handleTcpMessage(); //process TCP queue for all other messages
	string encapsulateFileList(); //LEADER ONLY: encode fileList into byte format
	void decapsulateFileList(string payload); //update file positions and remove files that have 0 copies
	string encapsulateMessage(map<PayloadType,string> payloads); //Combines membership list heartbeating with fileList info (master sends this only)
	string decapsulateMessage(string payload); //handle file list and return regular part of the message
	void listLocalFiles(); //iterate over localFileList
	void listSDFSFileList(string sdfsfilename); //print all locations of sdfsfilename from fileList
	string populateSDFSFileList(MessageType type, string mem_list_to_send); //piggyback fileList to membershipList if coming from master
	void updateFileList(string sdfsfilename, int nodePosition); //LEADER ONLY: add new position for file in fileList
	void checkFileListConsistency(); //LEADER ONLY: have leader decide where to replicate files with not enough copies

	bool checkLeaderExist(); //return if leader exists
	bool findWillBeLeader(); //min on hashring should be leader, also update node's successor while checking the ring
	void leaderCreateHashRing(); //leader creates new hashring from its membership list
	void proposeToBeLeader(); //TCP ELECTION msg to successor
	void electionMessageHandler(Messages messages); //process TCP ELECTION messages using ring leader election algo
	void restartElection(); //reset leader info
	void setUpLeader(string message, bool pending); //setup leader
	void replicateKeys();
	void resetMapleJuice();
	void handleMaplejuiceQ();
private:
	string populateMembershipMessage(); //membershipList to string based on mode type
	string populateIntroducerMembershipMessage(); //entire membership list to string
	void handleUdpMessage(string message); //handle each UDP message
	int getPositionOnHashring(); //returns hashring position
	int updateHashRing(); //go through membershipList and add nodes to hashRing if not already there
	void processHeartbeat(string message); //update local membership list from a heartbeat message
	vector<tuple<string,string, string>> getRandomNodesToGossipTo(); //pick targets for multicast
};

void computeAndPrintBW(Node * n, double diff); //print bandwidth
void debugSDFSFileList(Node * n); //debug function
void debugMembershipList(Node * n); //debug function

#endif //NODE_H
