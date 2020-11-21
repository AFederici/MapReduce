#include "../inc/Node.h"

void *runUdpServer(void *udpSocket)
{
	UdpSocket* udp;
	udp = (UdpSocket*) udpSocket;
	udp->bindServer(PORT);
	pthread_exit(NULL);
}

void *runTcpServer(void *tcpSocket)
{
	TcpSocket* tcp;
	tcp = (TcpSocket*) tcpSocket;
	tcp->bindServer(TCPPORT);
	pthread_exit(NULL);
}

void *runTcpSender(void *tcpSocket)
{
	TcpSocket* tcp;
	tcp = (TcpSocket*) tcpSocket;
	while (1) {
		while (!tcp->mapleMessages.empty()) {
			vector<string> msgSplit = splitString(tcp->mapleMessages.front(), "::");
			string removeSender = tcp->mapleMessages.front().substr(msgSplit[0].size() + 2);
			cout << "[TEST] " << removeSender << endl;
			Messages msg(CHUNK, removeSender);
			tcp->sendMessage(msgSplit[0], TCPPORT, msg.toString());
			tcp->mapleMessages.pop();
		}
		while (!tcp->pendSendMessages.empty()) {
			vector<string> msgSplit = splitString(tcp->pendSendMessages.front(), "::");
			if (msgSplit.size() >= 5) {
				string nodeIP = msgSplit[0], localfilename = msgSplit[1], overwrite = msgSplit[4];
				string sdfsfilename = msgSplit[2], remoteLocalfilename = msgSplit[3];
				cout << "[DOSEND] nodeIP " << nodeIP << ", localfilename " << localfilename;
				cout << ", sdfsfilename " << sdfsfilename << ", remoteLocalfilename " << remoteLocalfilename << endl;
				if (msgSplit.size() == 6){
					//IP, exec, file, start, prefix, end
					int start = stoi(msgSplit[3]);
					int end = stoi(msgSplit[5]);
					cout << "[CHUNK] sending : " << sdfsfilename << " from " << msgSplit[3] << " to " << msgSplit[5] << endl;
					tcp->sendLines(nodeIP, TCPPORT, localfilename, sdfsfilename, msgSplit[4], start, end); //exec, file, start, end
				}
				else tcp->sendFile(nodeIP, TCPPORT, localfilename, sdfsfilename, remoteLocalfilename, overwrite);
			}
			tcp->pendSendMessages.pop();
		}
	}
	pthread_exit(NULL);
}

void *runSenderThread(void *node)
{
	// acquire node object
	Node *nodeOwn = (Node *) node;
	nodeOwn->activeRunning = true;

	// heartbeat to introducer to join the system
	Member introducer(getIP(INTRODUCER), PORT);
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
		nodeOwn->handleTcpMessage();

		// 6. check leader (If hashRing is sent via heartbeat, then we have a leader)
		if (!nodeOwn->checkLeaderExist()) { // If no leader
			nodeOwn->tcpElectionProcessor();
			if (nodeOwn->findWillBeLeader()) {
				if (nodeOwn->localTimestamp - nodeOwn->electedTime > T_election)  { // when entering to stable state
					if (nodeOwn->localTimestamp - nodeOwn->proposedTime > T_election) {
						nodeOwn->proposeToBeLeader();
						nodeOwn->proposedTime = nodeOwn->localTimestamp;
					}
				}
			}
		}

		// 7. bandwidth and mode switch handled (optional)
		time_t endTimestamp;
		time(&endTimestamp);
		double diff = difftime(endTimestamp, nodeOwn->startTimestamp);
		computeAndPrintBW(nodeOwn, diff);
		if (nodeOwn->prepareToSwitch) {
			cout << "[SWITCH] I am going to swtich my mode in " << T_switch << "s" << endl;
			nodeOwn->SwitchMyMode();
		} else {
			usleep(T_period);
		}
	}
	pthread_exit(NULL);
}
