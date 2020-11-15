#include "../inc/Node.h"

/**
 * 
 * runUdpServer: Enqueue each heartbeat it receives
 *
 **/
void *runUdpServer(void *udpSocket) 
{
	// acquire UDP object
	UdpSocket* udp;
	udp = (UdpSocket*) udpSocket;
	udp->bindServer(PORT);
	pthread_exit(NULL);
}

/**
 * 
 * runTcpServer: Enqueue each request it receives
 *
 **/
void *runTcpServer(void *tcpSocket)
{
	TcpSocket* tcp;
	tcp = (TcpSocket*) tcpSocket;
	tcp->bindServer(TCPPORT);
	pthread_exit(NULL);
}

vector<string> splitString(string s, string delimiter){
	vector<string> result;
	size_t pos_start = 0, pos_end, delim_len = delimiter.length();
	string token;

	while ((pos_end = s.find (delimiter, pos_start)) != string::npos) {
		token = s.substr (pos_start, pos_end - pos_start);
		pos_start = pos_end + delim_len;
		result.push_back (token);
	}

	result.push_back (s.substr (pos_start));
	return result;
}

void *runTcpSender(void *tcpSocket)
{
	TcpSocket* tcp;
	tcp = (TcpSocket*) tcpSocket;
	while (1) {
		while (!tcp->pendSendMessages.empty()) {
			vector<string> msgSplit = splitString(tcp->pendSendMessages.front(), "::");
			if (msgSplit.size() >= 4) {
				string nodeIP = msgSplit[0];
				string localfilename = msgSplit[1];
				string sdfsfilename = msgSplit[2];
				string remoteLocalfilename = msgSplit[3];
				cout << "[DOSEND] nodeIP " << nodeIP << ", localfilename " << localfilename;
				cout << ", sdfsfilename " << sdfsfilename << ", remoteLocalfilename " << remoteLocalfilename << endl;
				tcp->sendFile(nodeIP, TCPPORT, localfilename, sdfsfilename, remoteLocalfilename);
			}
			tcp->pendSendMessages.pop();
		}
	}
	pthread_exit(NULL);
}

void testMessages(UdpSocket* udp)
{
	sleep(2);
	for (int j = 0; j < 4; j++) {
		udp->sendMessage("127.0.0.1", PORT, "test message "+to_string(j));
	}
	sleep(1);
}

/**
 * 
 * runSenderThread: 
 * 1. handle messages in queue
 * 2. merge membership list
 * 3. prepare to send heartbeating
 * 4. do gossiping
 *
 **/
void *runSenderThread(void *node) 
{
	// acquire node object
	Node *nodeOwn = (Node *) node;

	nodeOwn->activeRunning = true;

	// step: joining to the group -> just heartbeating to introducer
	Member introducer(INTRODUCER, PORT);
	nodeOwn->joinSystem(introducer);

	while (nodeOwn->activeRunning) {
		
		// 1. deepcopy and handle queue, and
		// 2. merge membership list
		nodeOwn->listenToHeartbeats();
		
		// Volunteerily leave
		if(nodeOwn->activeRunning == false){
			pthread_exit(NULL);
		}

		//add failure detection in between listening and sending out heartbeats
		nodeOwn->failureDetection();
		
		// keep heartbeating
		nodeOwn->localTimestamp++;
		nodeOwn->heartbeatCounter++;
		nodeOwn->updateNodeHeartbeatAndTime();
		
		// 3. prepare to send heartbeating, and 
		// 4. do gossiping
		nodeOwn->heartbeatToNode();

		// 5. check for regular TCP messages
		nodeOwn->processRegMessages();

		// 6. check leader (If hashRing is sent via heartbeat, then we have a leader)
		if (!nodeOwn->checkLeaderExist()) { // If no leader
			nodeOwn->processTcpMessages();
			if (nodeOwn->findWillBeLeader()) {
				//cout << "Try to propose to be leader" << endl;
				if (nodeOwn->localTimestamp-nodeOwn->electedTime > T_election)  { // when entering to stable state
					if (nodeOwn->localTimestamp-nodeOwn->proposedTime > T_election) {
						nodeOwn->proposeToBeLeader();
						nodeOwn->proposedTime = nodeOwn->localTimestamp;
					}
				}
			}
		}

		// for debugging
		//nodeOwn->debugMembershipList();
		time_t endTimestamp;
		time(&endTimestamp);
		double diff = difftime(endTimestamp, nodeOwn->startTimestamp);
		nodeOwn->computeAndPrintBW(diff);
#ifdef LOG_VERBOSE
		cout << endl;
#endif
		if (nodeOwn->prepareToSwitch) {
			cout << "[SWITCH] I am going to swtich my mode in " << T_switch << "s" << endl;
			nodeOwn->SwitchMyMode();
		} else {
			usleep(T_period);
		}
	}

	pthread_exit(NULL);
}