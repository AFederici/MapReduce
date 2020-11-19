#include "../inc/Node.h"

//TODO, single file for each key, and ussing an append to each replica of it?
//keep a local map, if you find a new key, just let the master know about it and hold off on results in buffer until it exists
Node::Node(){
	udpServent = new UdpSocket();
	tcpServent = new TcpSocket();
	hashRing = new HashRing();
	mapleRing = new HashRing();
	localTimestamp = 0;
	heartbeatCounter = 0;
	runningMode = ALL2ALL;
	activeRunning = false;
	prepareToSwitch = false;
	logWriter = new Logger(LOGGING_FILE_NAME);
	leaderPosition = -1;
	proposedTime = 0;
	electedTime = 0;
	joinTimestamp = "";
	mapleExe = "";
	sdfsPre = "";
	possibleSuccessorIP = "";
	leaderIP = "";
	leaderPort = "";
	isBlackout = true;
	struct sigaction sa;
	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
}

Node::Node(ModeType mode) : Node() { runningMode = mode; }

void Node::startActive()
{
	membershipList.clear();
	restartElection();
	// inserting its own into the list
	time(&startTimestamp);
	string startTime = updateNodeHeartbeatAndTime();
	debugMembershipList(this);
	joinTimestamp = startTime; // for hashRing
	getPositionOnHashring(); // update its hashRingPosition
}

string Node::updateNodeHeartbeatAndTime()
{
	string startTime = ctime(&startTimestamp);
	startTime = startTime.substr(0, startTime.find("\n"));
	tuple<string, string, string> keyTuple(nodeInformation.ip, nodeInformation.port,startTime);
	tuple<int, int, int> valueTuple(heartbeatCounter, localTimestamp, 0);
	this->membershipList[keyTuple] = valueTuple;
	return startTime;
}

string Node::populateMembershipMessage()
{
	//The string we send will be seperated line by line --> IP,PORT,HeartbeatCounter,FailFlag
	string mem_list_to_send = "";
	switch (this->runningMode)
	{
		case GOSSIP:
			return populateIntroducerMembershipMessage(); // code re-use
		default:
			string startTime = ctime(&startTimestamp);
			startTime = startTime.substr(0, startTime.find("\n"));
			mem_list_to_send += nodeInformation.ip + "," + nodeInformation.port + "," + startTime + ",";
			mem_list_to_send += to_string(heartbeatCounter) + "," + to_string(0) + "\n";
			return mem_list_to_send;
	}
}

string Node::populateIntroducerMembershipMessage() {
	string mem_list_to_send = "";
	for (auto& element: this->membershipList) {
		tuple<string, string, string> keyTuple = element.first;
		tuple<int, int, int> valueTuple = element.second;
		mem_list_to_send += get<0>(keyTuple) + "," + get<1>(keyTuple) + "," + get<2>(keyTuple) + ",";
		mem_list_to_send += to_string(get<0>(valueTuple)) + "," + to_string(get<2>(valueTuple)) + "\n";
	}
	return mem_list_to_send;
}

int Node::heartbeatToNode()
{
	string msg;
	string mem_list_to_send = populateMembershipMessage();
	vector<tuple<string,string,string>> targetNodes = getRandomNodesToGossipTo();
#ifdef LOG_VERBOSE
	cout << "pick " << targetNodes.size() << " of " << this->membershipList.size()-1;
	cout << " members" << endl;
#endif
	for (uint i=0; i<targetNodes.size(); i++) {
		Member destination(get<0>(targetNodes[i]), get<1>(targetNodes[i]));
		string message = "["+to_string(this->localTimestamp)+"] node "+destination.ip+"/"+destination.port+"/"+get<2>(targetNodes[i]);
#ifdef LOG_VERBOSE
		cout << "[Gossip]" << message.c_str() << endl;
#endif
		this->logWriter->printTheLog(GOSSIPTO, message);
		if (isLeader) {
			if (isBlackout) msg = populateSDFSFileList(LEADERPENDING, mem_list_to_send);
			else msg = populateSDFSFileList(LEADERHEARTBEAT, mem_list_to_send);
		}
		else msg = populateSDFSFileList(HEARTBEAT, mem_list_to_send);
		udpServent->sendMessage(destination.ip, destination.port, msg);
	}
	return 0;
}

int Node::failureDetection(){
	//1. check local membership list for any timestamps whose curr_time - timestamp > T_fail
	//2. If yes, mark node as local failure, update fail flag to 1 and update timestamp to current time
	//3. for already failed nodes, check to see if curr_time - time stamp > T_cleanup
	//4. If yes, remove node from membership list
	vector<tuple<string,string,string>> removedVec;
	for(auto& element: this->membershipList){
		tuple<string,string,string> keyTuple = element.first;
		tuple<int, int, int> valueTuple = element.second;
#ifdef LOG_VERBOSE
		cout << "checking " << get<0>(keyTuple) << "/" << get<1>(keyTuple) << "/" << get<2>(keyTuple) << endl;
#endif
		if ((get<0>(keyTuple).compare(nodeInformation.ip) == 0) && (get<1>(keyTuple).compare(nodeInformation.port) == 0)) {
#ifdef LOG_VERBOSE
			cout << "do not check itself" << endl;
#endif
			continue;
		}
		//node has not failed
		if(get<2>(valueTuple) == 0){
			//timeout passed, set as failed
			if(localTimestamp - get<1>(valueTuple) > T_timeout){
				//cout << "Got " << get<0>(keyTuple) << "/" << get<1>(keyTuple) << "/" << get<2>(keyTuple) << endl;
				//cout << "local time " << localTimestamp << " vs. " << get<1>(valueTuple) << endl;
				get<1>(this->membershipList[keyTuple]) = localTimestamp;
				get<2>(this->membershipList[keyTuple]) = 1;
				string message = "["+to_string(this->localTimestamp)+"] node "+get<0>(keyTuple)+"/"+get<1>(keyTuple)+"/"+get<2>(keyTuple)+": Local Failure";
				cout << "[FAIL]" << message.c_str() << endl;
				this->logWriter->printTheLog(FAIL, message);
				if(isLeader){
					Member deletedNode(get<0>(keyTuple), get<1>(keyTuple));
					int deletedNodePostion = hashingId(deletedNode, get<2>(keyTuple));
					hashRing->removeNode(deletedNodePostion);
					//for each file, remove the deleted node from its location vector
					for (auto& element: fileList) {
						vector<int> newEntry;
						for(unsigned int i = 0; i < element.second.size(); i++){
							if(element.second[i] != deletedNodePostion){
								newEntry.push_back(element.second[i]);
							}
						}
						fileList[element.first] = newEntry;
					}

					//////////////////////////////////////////////////////
					//1) remove from HashRing
					//2) if processing, reassign
					//2a) if no extra nodes, assign to successor, else add new node to ring
					//3) if a sender, reassign replica holders as new senders for each thing sent
					vector<tuple<string,string,string>> aliveNodes;
					for (auto &e : membershipList) aliveNodes.push_back(e.first);
					vector<tuple<string,string,string>> mapleNodes;
					int nextId;
					if ((mapleRing->nodePositions.size()-1) == hashRing->nodePositions.size()){
						nextId = mapleRing->getSuccessor(deletedNodePostion);
					} else {
						while (1){
							Member m(get<0>(mapleNodes[0]), get<1>(mapleNodes[0]));
							nextId = hashingId(m, get<2>(mapleNodes[0]));
							mapleNodes = randItems(1, aliveNodes);
							if (mapleRing->getValue(nextId).compare("No node found") != 0) continue;
							break;
						}
						mapleRing->addNode(get<0>(mapleNodes[0]), nextId);
					}
					mapleRing->removeNode(deletedNodePostion);

					auto vecCopy(mapleProcessing[get<0>(keyTuple)]);
					mapleProcessing[get<0>(mapleNodes[0])] = vecCopy;
					mapleProcessing.erase(get<0>(keyTuple));

					for (auto &e : mapleSending[get<0>(keyTuple)]){
						vector<int> temp = randItems(1, fileList[get<0>(e)]);
						mapleSending[hashRing->getValue(temp[0])].push_back(make_tuple(get<0>(e), get<1>(e)));
						//mapleRing->getValue(id) + "::" maple_exe + ":: " + s + "::" + sdfs_pre;
						string mapleS = hashRing->getValue(temp[0]) + "::" + mapleExe + "::" + get<0>(e)+ "::" + get<1>(e)+ "::" + sdfsPre;
						tcpServent->mapleMessages.push(mapleS);
					}


					// chech if the failure is the sender in pending requests
					for (auto& senders: pendingSenderRequests) {
						string sdfsfilename = senders.first;
						tuple<string,string,string> sender = senders.second;
						if ((get<0>(keyTuple).compare(get<0>(sender))==0) &&
							get<0>(pendingRequestSent[sdfsfilename]) &&
							(get<0>(pendingRequests[sdfsfilename])!=-1))
						{
							// it sent out, and is not finished, cannot help
							// we lost the data from the client
							cout << "[PUT] client itself fails, we cannot help, remove request" << endl;
							isBlackout = false;
							pendingRequests.erase(sdfsfilename);
							pendingRequestSent.erase(sdfsfilename);
							continue;
						}
						if ((get<0>(keyTuple).compare(get<1>(sender))==0) &&
							get<1>(pendingRequestSent[sdfsfilename]) &&
							(get<1>(pendingRequests[sdfsfilename])!=-1))
						{
							// the sender fails during 2nd pass
							// replace the sent
							cout << "[PUT] One of the sender " << get<0>(keyTuple) << " failed, try again" << endl;
							if (get<2>(pendingRequests[sdfsfilename])!=-1) {
								tuple<int, int, int>(get<0>(pendingRequestSent[sdfsfilename]), false, get<2>(pendingRequestSent[sdfsfilename]));
							} else {
								pendingRequests.erase(sdfsfilename);
							}
							continue;
						}
						if ((get<0>(keyTuple).compare(get<2>(sender))==0) &&
							get<2>(pendingRequestSent[sdfsfilename]) &&
							(get<2>(pendingRequests[sdfsfilename])!=-1))
						{
							// it sent out, but replicates are failed
							// restart again
							cout << "[PUT/REREPLICATE] The sender " << get<0>(keyTuple) << " failed, try again" << endl;
							pendingRequests.erase(sdfsfilename);
						}
					}

				}

			}
		}
		//check for cleanup on already failed nodes
		else{
			if(localTimestamp - get<1>(valueTuple) > T_cleanup){
				auto iter = this->membershipList.find(keyTuple);
				if (iter != this->membershipList.end()) {
					removedVec.push_back(keyTuple);
				}
			}
		}
	}

	// O(c*n) operation, but it ensures safety
	bool leaderRemoved = false;
	for (uint i=0; i<removedVec.size(); i++) {
		auto iter = this->membershipList.find(removedVec[i]);
		if (iter != this->membershipList.end()) {

			if (leaderIP.compare(get<0>(removedVec[i]))==0) { // this is the leader
				leaderRemoved = true;
				cout << "[ELECTION] leader " << leaderIP << " is removed" << endl;
			}

			this->membershipList.erase(iter);
			string message = "["+to_string(this->localTimestamp)+"] node "+get<0>(removedVec[i])+"/"+get<1>(removedVec[i])+"/"+get<2>(removedVec[i])+": REMOVED FROM LOCAL MEMBERSHIP LIST";
			cout << "[REMOVE]" << message.c_str() << endl;
			this->logWriter->printTheLog(REMOVE, message);
			//debugMembershipList(this);
		}
	}
	if (this->membershipList.size()==1 || leaderRemoved) { // Only me or leader failed, restart leader election
		if (checkLeaderExist()) { // restart if we have a leader
			restartElection();
		}
	}
	return 0;
}

int Node::joinSystem(Member introducer)
{
	string mem_list_to_send = populateMembershipMessage();
	string msg = populateSDFSFileList(JOIN, mem_list_to_send);
	string message = "["+to_string(this->localTimestamp)+"] sent a request to "+introducer.ip+"/"+introducer.port+", I am "+nodeInformation.ip+"/"+nodeInformation.port;
	cout << "[JOIN]" << message.c_str() << endl;
	this->logWriter->printTheLog(JOINGROUP, message);
	udpServent->sendMessage(introducer.ip, introducer.port, msg);
	return 0;
}

int Node::requestSwitchingMode()
{
	string message = nodeInformation.ip+","+nodeInformation.port;
	string msg = populateSDFSFileList(SWREQ, message);
	for(auto& element: this->membershipList) {
		tuple<string,string,string> keyTuple = element.first;
		cout << "[SWITCH] sent a request to " << get<0>(keyTuple) << "/" << get<1>(keyTuple) << endl;
		udpServent->sendMessage(get<0>(keyTuple), get<1>(keyTuple), msg);
	}
	return 0;
}

int Node::SwitchMyMode() {
	sleep(T_switch); // wait for a while
	udpServent->qMessages = queue<string>(); // empty all messages
	switch (this->runningMode) {
		case GOSSIP: {
			this->runningMode = ALL2ALL;
			cout << "[SWITCH] === from gossip to all-to-all ===" << endl;
			break;
		}
		case ALL2ALL: {
			this->runningMode = GOSSIP;
			cout << "[SWITCH] === from all-to-all to gossip ===" << endl;
			break;
		}
		default:
			break;
	}
	prepareToSwitch = false; // finishing up
	return 0;
}

int Node::listenToHeartbeats() {
	// 1. deepcopy and handle queue
	queue<string> qCopy(udpServent->qMessages);
	udpServent->qMessages = queue<string>();
	int size = qCopy.size();
	//cout << "Got " << size << " messages in the queue" << endl;
	//cout << "checking queue size " << nodeOwn->udpServent->qMessages.size() << endl;
	for (int j = 0; j < size; j++) {
		//cout << qCopy.front() << endl;
		handleUdpMessage(qCopy.front());
		if(this->activeRunning == false) return 0;
		qCopy.pop();
	}
	return 0;
}

/*
 * Take a hearbeat message, if the member doesn't exist add it, update hashring, and disseminate out memberList
 * If it exists, check for failure, and if there is update fail flag, otherwise try ot update heartbeat
*/
void Node::processHeartbeat(string message) {
	bool changed = false;
	vector<string> incomingMembershipList = splitString(message, "\n");
	vector<string> membershipListEntry;
	for(string list_entry: incomingMembershipList){
#ifdef LOG_VERBOSE
		cout << "handling with " << list_entry << endl;
#endif
		if (list_entry.size() == 0) {
			continue;
		}
		membershipListEntry.clear();
		membershipListEntry = splitString(list_entry, ",");
		if (membershipListEntry.size() != 5) continue;

		int incomingHeartbeatCounter = stoi(membershipListEntry[3]);
		int failFlag = stoi(membershipListEntry[4]);
		tuple<string,string,string> mapKey(membershipListEntry[0], membershipListEntry[1], membershipListEntry[2]);

		if ((get<0>(mapKey).compare(nodeInformation.ip) == 0) && (get<1>(mapKey).compare(nodeInformation.port) == 0)) {
			// Volunteerily leave if you are marked as failed
			if(failFlag == 1){
				this->activeRunning = false;
				string message = "["+to_string(this->localTimestamp)+"] node "+this->nodeInformation.ip+"/"+this->nodeInformation.port+" is left";
				cout << "[VOLUNTARY LEAVE]" << message.c_str() << endl;
				this->logWriter->printTheLog(LEAVE, message);
				return;
			}
#ifdef LOG_VERBOSE
			cout << "do not check itself heartbeat" << endl;
#endif
			continue;
		}

		map<tuple<string,string,string>, tuple<int, int, int>>::iterator it;
		it = this->membershipList.find(mapKey);
		if (it == this->membershipList.end() && failFlag == 0) {
			tuple<int, int, int> valueTuple(incomingHeartbeatCounter, localTimestamp, failFlag);
			this->membershipList[mapKey] = valueTuple;
			updateHashRing();
			string message = "["+to_string(this->localTimestamp)+"] new node "+get<0>(mapKey)+"/"+get<1>(mapKey)+"/"+get<2>(mapKey)+" is joined";
			cout << "[JOIN]" << message.c_str() << endl;
			this->logWriter->printTheLog(JOINGROUP, message);
			changed = true;
		} else if(it != this->membershipList.end()) {
			// update heartbeat count and local timestamp if fail flag of node is not equal to 1. If it equals 1, we ignore it.
			if(get<2>(this->membershipList[mapKey]) != 1){
				//if incoming membership list has node with fail flag = 1, but fail flag in local membership list = 0, we have to set fail flag = 1 in local
				switch (this->runningMode) {
					case GOSSIP: {
						if(failFlag == 1){
							get<2>(this->membershipList[mapKey]) = 1;
							get<1>(this->membershipList[mapKey]) = localTimestamp;
							string message = "["+to_string(this->localTimestamp)+"] node "+get<0>(mapKey)+"/"+get<1>(mapKey)+"/"+get<2>(mapKey)+": Disseminated Failure";
							cout << "[FAIL]" << message.c_str() << endl;
							this->logWriter->printTheLog(FAIL, message);
						}
						else{
							int currentHeartbeatCounter = get<0>(this->membershipList[mapKey]);
							if(incomingHeartbeatCounter > currentHeartbeatCounter){
								get<0>(this->membershipList[mapKey]) = incomingHeartbeatCounter;
								get<1>(this->membershipList[mapKey]) = localTimestamp;
								string message = "["+to_string(this->localTimestamp)+"] node "+get<0>(mapKey)+"/"+get<1>(mapKey)+"/"+get<2>(mapKey)+" from "+to_string(currentHeartbeatCounter)+" to "+to_string(incomingHeartbeatCounter);
#ifdef LOG_VERBOSE
								cout << "[UPDATE]" << message.c_str() << endl;
#endif
								this->logWriter->printTheLog(UPDATE, message);
							}
						}
						break;
					}
					default: { // ALL2ALL doesn't disseminate
						int currentHeartbeatCounter = get<0>(this->membershipList[mapKey]);
						if(incomingHeartbeatCounter > currentHeartbeatCounter){
							get<0>(this->membershipList[mapKey]) = incomingHeartbeatCounter;
							get<1>(this->membershipList[mapKey]) = localTimestamp;
							get<2>(this->membershipList[mapKey]) = failFlag;
							string message = "["+to_string(this->localTimestamp)+"] node "+get<0>(mapKey)+"/"+get<1>(mapKey)+"/"+get<2>(mapKey)+" from "+to_string(currentHeartbeatCounter)+" to "+to_string(incomingHeartbeatCounter);
#ifdef LOG_VERBOSE
							cout << "[UPDATE]" << message.c_str() << endl;
#endif
							this->logWriter->printTheLog(UPDATE, message);
						}
						break;
					}
				}
			}
		}
	}
	// If membership list changed in all-to-all, full membership list will be sent
	if(changed && this->runningMode == ALL2ALL) heartbeatToNode();
}

void Node::setUpLeader(string message, bool pending)
{
	string msg(message);
	vector<string> fields = splitString(msg, ",");
	if(fields.size() >= 3){
		Member leader(fields[0], fields[1]);
		leaderPosition = hashingId(leader, fields[2]);
		leaderIP = fields[0];
		leaderPort = fields[1];
	}
	leaderCreateHashRing(); // local copy of hashRing on each node

	if (pending != isBlackout) {
		if (isBlackout) {
			cout << "[BLACKOUT] Leader is ready now" << endl;
		} else {
			cout << "[BLACKOUT] Leader is busy now" << endl;
		}
	}
	if (pending) {
		isBlackout = true;
	} else {
		isBlackout = false;
	}
}

/**
 * given a string message which contains a membership list, we will take the string, split it by returns, and then split it by commas, to then compare the heartbeat counters
 * of each IP,PORT,timestamp tuple with the membership list of the receiving Node.
 * Found help on how to do string processing part of this at https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
 */
void Node::handleUdpMessage(string message){
	//cout << "handleUdpMessage " << message << endl;
	string deMeg = decapsulateMessage(message);
	bool pending = true;
	//cout << "handleUdpMessage deMeg " << deMeg << endl;
	Messages msg(deMeg);
	switch (msg.type) {
		case LEADERHEARTBEAT: // Note: not for Gossip-style, only All-to-All
			//cout << "LEADERHEARTBEAT: " << msg.payload << endl;
			pending = false;
		case LEADERPENDING: setUpLeader(msg.payload, pending);
		case HEARTBEAT:
		case JOINRESPONSE:{
			processHeartbeat(msg.payload);
			break;
		}
		case JOIN:{
			// introducer checks collision here
			vector<string> fields = splitString(msg.payload, ",");
			if(fields.size() >= 3){
				Member member(fields[0], fields[1]);
				int checkPosition = hashingId(member, fields[2]);
				if (checkHashNodeCollision(checkPosition)) {
					string response = populateSDFSFileList(JOINREJECT, "");
					udpServent->sendMessage(fields[0], fields[1], response);
				} else {
					string introducerMembershipList;
					introducerMembershipList = populateIntroducerMembershipMessage();
					string response = populateSDFSFileList(JOINRESPONSE, introducerMembershipList);
					udpServent->sendMessage(fields[0], fields[1], response);
				}
			}
			break;
		}
		case SWREQ: {
			// got a request, send an ack back
			vector<string> fields = splitString(msg.payload, ",");
			if (fields.size() == 2) {
				cout << "[SWITCH] got a request from "+fields[0]+"/"+fields[1] << endl;
				string messageReply = nodeInformation.ip+","+nodeInformation.port;
				//Messages msgReply(SWRESP, messageReply);
				string msgReply = populateSDFSFileList(SWRESP, messageReply);
				udpServent->sendMessage(fields[0], fields[1], msgReply);
				prepareToSwitch = true;
			}
			break;
		}
		case SWRESP: {
			// got an ack
			vector<string> fields = splitString(msg.payload, ",");
			if (fields.size() == 2) {
				cout << "[SWITCH] got an ack from "+fields[0]+"/"+fields[1] << endl;
			}
			break;
		}
		case JOINREJECT: {
			// TODO: the node should leave
			cout << "[JOINREJECT] There is a collision, and I have to leave..." << endl;
			this->activeRunning = false;
			pthread_exit(NULL);
			break;
		}
		default:
			break;
	}
	//debugMembershipList(this);
}

int Node::getPositionOnHashring(){
	hashRingPosition = hashingId(nodeInformation, joinTimestamp);
	cout << "[ELECTION] This node is at hash position: " << hashRingPosition << endl;
	return 0;
}

int Node::updateHashRing(){
	bool needToUpdate = true;
	for(auto& it: membershipList){
		needToUpdate = true;
		string ip = get<0>(it.first);
		for(int i: hashRing->nodePositions){
			if(ip.compare(hashRing->getValue(i)) == 0){
				needToUpdate = false;
				break;
			}
		}
		if(needToUpdate){
			Member toBeInserted(ip, get<1>(it.first));
			int hashPosition = hashingId(toBeInserted,  get<2>(it.first));
			hashRing->addNode(ip, hashPosition);
		}
	}
	return 0;
}

bool Node::checkLeaderExist()
{
	return leaderPosition != -1;
}

bool Node::checkHashNodeCollision(int checkPosition)
{
	// if True, the position is full
	for (auto& element: this->membershipList) {
		tuple<string, string, string> keyTuple = element.first;
		Member member(get<0>(keyTuple), get<1>(keyTuple));
		if (nodeInformation.ip.compare(member.ip)==0) { // myself, skip it
			continue;
		}
		int pos = hashingId(member, get<2>(keyTuple));
		if (pos == checkPosition) {
			return true;
		}
	}
	return false;
}

bool Node::findWillBeLeader()
{
	bool beLeader = true;
	vector<int> positions;
    vector<string> ipAddresses;
	if (membershipList.size() > 1) { // only 1 member does not need the leader
		for (auto& element: this->membershipList) {
			tuple<string, string, string> keyTuple = element.first;
			Member member(get<0>(keyTuple), get<1>(keyTuple));
			int pos = hashingId(member, get<2>(keyTuple));
			if (pos < hashRingPosition) {
				//cout << get<0>(keyTuple) << " with id " << pos << " is smaller" << endl;
				beLeader = false;
			}
			if (nodeInformation.ip.compare(get<0>(keyTuple))!=0) {
				int posNext = (pos + (HASHMODULO-hashRingPosition)) % HASHMODULO;
				positions.push_back(posNext);
				ipAddresses.push_back(get<0>(keyTuple));
			}
		}
	} else {
		beLeader = false;
	}

	if (positions.size() > 0) {
		int index = 0;
		int possibleSuccessor = positions[index];
		for (uint i=1; i<positions.size(); i++) {
			if (positions[i] < possibleSuccessor) {
				possibleSuccessor = positions[i];
				index = i;
			}
		}
		//cout << "[ELECTION] My Possible Successor is " << ipAddresses[index] << endl;
		possibleSuccessorIP = ipAddresses[index];
	}

	return beLeader;
}

void Node::restartElection() // haven't tested yet
{
	cout << "[ELECTION] No leader now, restart election..." << endl;
	electedTime = localTimestamp;
	isLeader = false;
	leaderPosition = -1;
	leaderIP = "";
	leaderPort = "";
}

void Node::leaderCreateHashRing() {
	hashRing->clear();
	for (auto& element: this->membershipList) { // update hashRing
		tuple<string, string, string> keyTuple = element.first;
		Member member(get<0>(keyTuple), get<1>(keyTuple));
		int pos = hashingId(member, get<2>(keyTuple));
		hashRing->addNode(get<0>(keyTuple), pos);
	}
}

void Node::proposeToBeLeader() {
	Messages msg(ELECTION, to_string(hashRingPosition));
	cout << "[ELECTION] Propose to be leader, send to " << possibleSuccessorIP << endl;
 	tcpServent->sendMessage(possibleSuccessorIP, TCPPORT, msg.toString());
}

void Node::electionMessageHandler(Messages messages)
{
	switch (messages.type) {
		case ELECTION: { // check id
			int currentId = stoi(messages.payload);
			if (hashRingPosition > currentId) {
				//incoming is smaller, just forward
				cout << "[ELECTION] Got Election, agree on voting: " << messages.payload << endl;
				tcpServent->sendMessage(possibleSuccessorIP, TCPPORT, messages.toString());
			} else if (hashRingPosition < currentId) {
				//incoming is biger, replace and send it
				cout << "[ELECTION] Got Election, against this voting " << messages.payload;
				cout << ", and using my id " << hashRingPosition << endl;
				Messages msg(ELECTION, to_string(hashRingPosition));
				tcpServent->sendMessage(possibleSuccessorIP, TCPPORT, msg.toString());
			} else { // finish 1st pass
				cout << "[ELECTION] Got Election, everyone voted on me and start acking" << endl;
				Messages msg(ELECTIONACK, to_string(hashRingPosition));
				tcpServent->sendMessage(possibleSuccessorIP, TCPPORT, msg.toString());
			}
			break;
		}
		case ELECTIONACK: {
			int currentId = stoi(messages.payload);
			if (hashRingPosition == currentId) { // finish 2 pass
				cout << "[ELECTION] I am the leader now" << endl;
				isBlackout = false;
				leaderPosition = hashRingPosition;
				isLeader = true;
				leaderIP = nodeInformation.ip;
				leaderPort = nodeInformation.port;
				leaderCreateHashRing();
			} else {
				// Not me, just forward
				cout << "[ELECTION] Pass ACK " << messages.payload << endl;
				tcpServent->sendMessage(possibleSuccessorIP, TCPPORT, messages.toString());
			}
			electedTime = localTimestamp; // update elected time
			cout << "[ELECTION] Elected at Local Time " << electedTime << endl;
			break;
		}
		default:
			break;
	}
}

void Node::tcpElectionProcessor()
{
	queue<string> qCopy(tcpServent->qMessages);
	tcpServent->qMessages = queue<string>();
	//cout << "Got " << size << " TCP messages" << endl;
	for (int j=0; j<qCopy.size(); j++) {
		//cout << qCopy.front() << endl;
		Messages msg(qCopy.front());
		//cout << "Has " << msg.type << " with " << msg.payload << endl;
		switch (msg.type) {
			case ELECTION:
			case ELECTIONACK: electionMessageHandler(msg);
			default: break;
		}
		qCopy.pop();
	}
}

void Node::updateFileList(string sdfsfilename, int nodePosition)
{
	if (isLeader) {
		vector<int> positions = fileList[sdfsfilename];
		bool existed = false;
		for (uint i=0; i<positions.size(); i++) {
			if (positions[i] == nodePosition) {
				existed = true;
			}
		}
		if (!existed) {
			positions.push_back(nodePosition);
		}
		vector<int> storedPositionsCopy(positions);
		fileList[sdfsfilename] = storedPositionsCopy;
	}
}

void Node::checkFileListConsistency(){
	if (!isLeader) return;
	if (membershipList.size() < 4) {
		cout << "[ERROR] The number of members are too small, we need at least 4" << endl;
		return;
	}
	for (auto& element: fileList) {
		if(element.second.size() < 4){
			//Need to rereplicate --> do this one at a time
			//First check the closest node, successor and predecessor
			int closestNodePostion = hashRing->locateClosestNode(element.first);
			int pred = hashRing->getPredecessor(closestNodePostion);
			int succ = hashRing->getSuccessor(closestNodePostion);
			int randomNode = hashRing->getRandomNode(tuple<int, int, int>(closestNodePostion, pred, succ));
			vector<int> nodesToCheck = {closestNodePostion, pred, succ, randomNode};
			for(unsigned int i = 0; i < nodesToCheck.size(); i++){
				if (!isInVector(element.second, nodesToCheck[i]))
				{
					string nodeInfo = hashRing->getValue(nodesToCheck[i]);
					Messages outMsg(DNSGET, nodeInfo + "::" + to_string(nodesToCheck[i]) + "::" + element.first + "::");
					tuple<int, int, int> request = pendingRequests[element.first];
					if(get<0>(request) != -1 || get<1>(request) != -1 || get<2>(request) != -1){
						cout << "on put " << get<0>(request) << "/" << get<1>(request) << "/" << get<2>(request) << endl;
						break;
					}
					pendingRequests[element.first] = tuple<int, int, int>(-1, -1, nodesToCheck[i]);
					pendingRequestSent[element.first] = tuple<int, int, int>(true, true, true);
					tcpServent->sendMessage(leaderIP, TCPPORT, outMsg.toString());
					break;
				}
			}
		}
	}

}

vector<tuple<string,string, string>> Node::getRandomNodesToGossipTo()
{
    vector<tuple<string, string, string>> availableNodesInfo;
    vector<tuple<string, string, string>> selectedNodesInfo;
    vector<int> indexList;
    int availableNodes = 0;
    for(auto& element: this->membershipList){
        tuple<string, string, string> keyPair = element.first;
        tuple<int, int, int> valueTuple = element.second;
        //dont gossip to self or failed nodes
        if(get<0>(keyPair).compare(this->nodeInformation.ip) && (get<2>(valueTuple) != 1)){
            availableNodesInfo.push_back(keyPair);
            indexList.push_back(availableNodes++);
        }
    }
    switch (this->runningMode) {
        case GOSSIP: {
            return randItems(N_b, availableNodesInfo);
        }
        //ALL2ALL
        default: {
            return availableNodesInfo;
        }
    }
}

void Node::handleTcpMessage()
{
	//Before we do anything here, we should have the leader check to see if the file list is consistent or not.
	checkFileListConsistency();
	queue<string> qCopy(tcpServent->regMessages);
	tcpServent->regMessages = queue<string>();
	int size = qCopy.size();
	//cout << "Got " << size << " TCP messages" << endl;
	for (int j=0; j<size; j++) {
		// cout << qCopy.front() << endl;
		vector<string> msgSplit = splitString(qCopy.front(), "::");
		if (msgSplit.size() < 1){
			qCopy.pop();
			continue;
		}
		string payload = "";
		for(uint k = 1; k < msgSplit.size(); k++){
			if(k == msgSplit.size() - 1) payload += msgSplit[k];
			else payload += msgSplit[k] + "::";
		}
		MessageType msgType = static_cast<MessageType>(stoi(msgSplit[0]));
		Messages msg(msgType, payload);
		vector<string> inMsg = splitString(msg.payload, "::");
		// cout << "Has " << msg.type << " with " << msg.payload << endl;
		switch (msg.type) {
			case JUICESTART: {
				if (mapleProcessing.size()) {tcpServent->regMessages.push(msg.toString()); break;}
				//TODO

				//cleanup tmp-prefix-* regardless as those were real temp files
			}
			case MAPLESTART: {
				//leader only function
				//currently running something, dont start a new phase
				if (mapleProcessing.size()) {tcpServent->regMessages.push(msg.toString()); break;}
				if (inMsg.size() >= 4){
					string mapleExe = inMsg[0], num_maples = inMsg[1], sdfsPre = inMsg[2], sdfs_dir = inMsg[3];
					int workers = stoi(num_maples);
					if (workers > hashRing->nodePositions.size()-1) workers = hashRing->nodePositions.size()-1;
					int total_lines = 0;
					vector<tuple<string,int>> directory;
					for (auto &e: fileSizes){
						if (strncmp(e.first.c_str(), sdfs_dir.c_str(), sdfs_dir.size()) == 0){
							directory.push_back(make_tuple(e.first, get<1>(e.second)));
							total_lines += get<1>(e.second);
						}
					}
					vector<tuple<string,string,string>> aliveNodes;
					for (auto &e : membershipList) aliveNodes.push_back(e.first);
					vector<tuple<string,string,string>> mapleNodes = randItems(workers, aliveNodes);
					for (auto &e : mapleNodes) {
						Member m(get<0>(e), get<1>(e));
						mapleRing->addNode(get<0>(e), hashingId(m, get<2>(e)));
					}
					int start = 0, id = 0;
					string s;
					for (auto &e: directory){
						start = 0;
						while (start < get<1>(e)){
							s = get<0>(e) + "::" + to_string(start);
							id = mapleRing->locateClosestNode(s);
							vector<int> temp = randItems(1, fileList[get<0>(e)]);
							mapleProcessing[mapleRing->getValue(id)].push_back(make_tuple(get<0>(e), to_string(start), mapleRing->getValue(temp[0])));
							mapleSending[mapleRing->getValue(temp[0])].push_back(make_tuple(get<0>(e), to_string(start)));
							string maplemsg = mapleRing->getValue(id) + "::" +mapleExe + "::" + s + "::" + sdfsPre;
							//IP, exec, file, start, prefix
							tcpServent->mapleMessages.push(maplemsg);
							start = start + T_maples;
						}
					}
				}
				break;
			}

			case CHUNK: {
				cout << "[CHUNK] " << "we will put sdfsfilename: " << inMsg[2] << " from chunk: " << inMsg[3];
				cout << " to node " << inMsg[0] << endl;
				int end = stoi(inMsg[3]) + T_maples;
				string sendMsg = msg.payload + "::" + to_string(end);
				this->tcpServent->pendSendMessages.push(sendMsg);
				break;
			}

			case CHUNKACK: {
				//IP, exec, start, temp, actual file, prefix
				if (!isLeader) {
					//forward to know that the file was put okay
					this->tcpServent->sendMessage(leaderIP, TCPPORT, msg.toString());
					int dataPipe[2];
					if (pipe(dataPipe)){ fprintf (stderr, "Pipe failed.\n"); break; }
					pid_t pid = fork();
					if (pid){ //parent process, DONT need to waitpid because of signal handler set up
					  close(dataPipe[1]);
					  handlePipe(dataPipe[0], inMsg[5]);
					} else if (pid < 0) {
					    fprintf (stderr, "Fork failed.\n"); break;
					} else { //child process
					  close(dataPipe[0]);
					  dup2(dataPipe[1], 1); //stdout -> write end of pipe
					  string execName = "./" + inMsg[4];
					  int status = execl(execName.c_str(),execName.c_str(),inMsg[3].c_str(),NULL);
					  if (status < 0) exit(status);
					}
					close(dataPipe[0]);close(dataPipe[1]);
					//go thorugh and process things from datapipe
					//if processing success, send out TCP MAPLEACK
					string match = "tmp-" + inMsg[5];
					int matchLen = match.size();
					struct dirent *entry = nullptr;
					DIR *dp = nullptr;
					if ((dp = opendir(".")) == nullptr) { cout << "tmp directory error " << match << endl; break; }
					while ((entry = readdir(dp))){
					    if (strncmp(entry->d_name, match.c_str(), matchLen) == 0){
							string searcher(entry->d_name);
							string target = searcher.substr(4);
							Messages outMsg(DNS, nodeInformation.ip + "::" + to_string(hashRingPosition) + "::" + target + "::" + entry->d_name + "::" + to_string(-1) + "::" + to_string(-1) + "::" + target + "::" + "0");
							cout << "[PUT] Got localfilename: " << entry->d_name << " with sdfsfilename: " << target << endl;
							tcpServent->sendMessage(leaderIP, TCPPORT, outMsg.toString());
						}
					}
					closedir(dp);
					string ackStr = inMsg[0] + "::" + inMsg[4] + "::" + inMsg[2]; //IP, file, chunk
					Messages ackMsg(MAPLEACK, ackStr);
					tcpServent->sendMessage(leaderIP, TCPPORT, ackMsg.toString());
					break;
				}
				vector<tuple<string,string>> temp;
				for (auto &e : mapleSending[inMsg[0]]){
					if (get<0>(e).compare(inMsg[4]) == 0){
						if (get<1>(e).compare(inMsg[2]) == 0){
							continue;
						}
					}
					temp.push_back(e);
				}
				if (temp.size()) mapleSending[inMsg[0]] = temp;
				else mapleSending.erase(inMsg[0]);
				break;
			}

			case MAPLEACK: {
				vector<tuple<string,string,string>> temp;
				for (auto &e : mapleProcessing[inMsg[0]]){
					if (get<0>(e).compare(inMsg[1]) == 0){
						if (get<1>(e).compare(inMsg[2]) == 0){
							continue;
						}
					}
					temp.push_back(e);
				}
				if (temp.size()) mapleProcessing[inMsg[0]] = temp;
				else mapleProcessing.erase(inMsg[0]);
				break;
			}

			case PUTACK: {
				if(inMsg.size() >= 4){
					string inMsgIP = inMsg[0], sdfsfilename = inMsg[1];
					string localfilename = inMsg[2], remoteLocalname = inMsg[3];
					cout << "[PUTACK] " << "inMsgIP: " << inMsgIP << " sdfsfilename: " << sdfsfilename << " localfilename: " << localfilename << endl;
					localFilelist[sdfsfilename] = localfilename;
					Messages outMsg(ACK, to_string(this->hashRingPosition)+"::"+sdfsfilename+"::"+remoteLocalname);
					this->tcpServent->sendMessage(inMsgIP, TCPPORT, outMsg.toString());
				}
				break;
			}
			case DELETE: {
				if (isLeader) {
					if(inMsg.size() >= 2){
						string inMsgIP = inMsg[0], sdfsfilename = inMsg[1];
						cout << "[DELETE] " << "inMsgIP: " << inMsgIP << " sdfsfilename: " << sdfsfilename << endl;
						localFilelist.erase(sdfsfilename);
						fileList.erase(sdfsfilename);
						fileSizes.erase(sdfsfilename);
						// This is TCP, so we don't need to ACK
					}
				}
				break;
			}
			case DNSGET: {
				if(isLeader){
					cout << "msg.payload " << msg.payload << endl;
					if(inMsg.size() >= 4){
						string inMsgIP = inMsg[0];
						int nodePosition = stoi(inMsg[1]);
						int selectedNodePosition = nodePosition;
						string sdfsfilename = inMsg[2], localfilename = inMsg[3];
						cout << "[DNSGET] Got " << "inMsgIP: " << inMsgIP << ", sdfsfilename: " << sdfsfilename << ", localfilename: " << localfilename << endl;
						vector<int> positions = fileList[sdfsfilename];
						if (positions.size() == 0) {
							// the file is not available
							cout << "[DNSGET] sdfsfilename " << sdfsfilename << " is not available" << endl;
							fileList.erase(sdfsfilename);
							fileSizes.erase(sdfsfilename);
							Messages outMsg(GETNULL, sdfsfilename+": the file is not available::");
							this->tcpServent->sendMessage(inMsgIP, TCPPORT, outMsg.toString());
							break;
						}
						cout << "[DNSGET] we have ";
						for (uint i=0; i<positions.size(); i++) { // pick any node other than the requested node
							cout << positions[i] << " ";
							if (positions[i]!=nodePosition) {
								selectedNodePosition = positions[i];
							}
						}
						cout << endl;
						cout << "[DNSGET] we picks " << selectedNodePosition << endl;
						pendingRequests[sdfsfilename] = tuple<int, int, int>(-1, -1, nodePosition);
						pendingRequestSent[sdfsfilename] = tuple<int, int, int>(true, true, true);
						string nodeIP = hashRing->getValue(selectedNodePosition);
						pendingSenderRequests[sdfsfilename] = tuple<string, string, string>("", "", nodeIP);
						Messages outMsg(REREPLICATEGET, to_string(nodePosition) + "::" + sdfsfilename+ "::" +localfilename);
						cout << "[DNSGET] Ask node " << nodeIP << " to replicate on pos ";
						cout << to_string(nodePosition) << endl;
						this->tcpServent->sendMessage(nodeIP, TCPPORT, outMsg.toString());
					}
				}
				break;
			}
			case DNS: {
				// TODO: finish DNS functionality here, send out DNSANS
				if(isLeader){
					// Check hashring, get positions and send out DNS ANS
					isBlackout = true;
					if(inMsg.size() >= 8){
						string inMsgIP = inMsg[0];
						int nodePosition = stoi(inMsg[1]);
						string sdfsfilename = inMsg[2];
						string localfilename = inMsg[3];
						long int size = stol(inMsg[4]);
						int lines = stoi(inMsg[5]);
						string overwriteFilename = inMsg[6];
						string overwrite = inMsg[7];
						cout << "[DNS] Got " << "inMsgIP: " << inMsgIP << ", sdfsfilename: " << sdfsfilename;
						cout << ", localfilename: " << localfilename << ", pos: " << nodePosition << endl;
						// update fileList, client itself is one of the replicas
						updateFileList(sdfsfilename, nodePosition);
						fileSizes[sdfsfilename] = make_tuple(size, lines);
						hashRing->debugHashRing();
						int closestNode = hashRing->locateClosestNode(sdfsfilename);
						int pred = hashRing->getPredecessor(closestNode);
						int succ = hashRing->getSuccessor(closestNode);
						if (hashRing->getValue(closestNode).compare(inMsgIP)==0) {
							closestNode = hashRing->getRandomNode(tuple<int, int, int>(closestNode, pred, succ));
							cout << "[DNS] we need one more node " << closestNode << endl;
						}
						if (hashRing->getValue(pred).compare(inMsgIP)==0) {
							pred = hashRing->getRandomNode(tuple<int, int, int>(closestNode, pred, succ));
							cout << "[DNS] we need one more node " << pred << endl;
						}
						if (hashRing->getValue(succ).compare(inMsgIP)==0) {
							succ = hashRing->getRandomNode(tuple<int, int, int>(closestNode, pred, succ));
							cout << "[DNS] we need one more node " << succ << endl;
						}
						cout << "[DNS] we have nodes [" << closestNode << " (closestNode), ";
						cout << pred << " (pred), " << succ << " (succ)], reply " << closestNode << endl;
						pendingRequests[sdfsfilename] = tuple<int, int, int>(closestNode, pred, succ);
						pendingRequestSent[sdfsfilename] = tuple<int, int, int>(true, false, false);
						pendingSenderRequests[sdfsfilename] = tuple<string, string, string>(inMsgIP, "", "");
						Messages outMsg(DNSANS, to_string(closestNode) + "::" + localfilename + "::" + sdfsfilename + "::" + overwriteFilename + "::" + overwrite);
						this->tcpServent->sendMessage(inMsgIP, TCPPORT, outMsg.toString());
					}
				}

				break;
			}
			case DNSANS:{
				// Read the answer and send a PUT msg to dest
				if(inMsg.size() >= 5){
					int nodePosition = stoi(inMsg[0]);
					// since we do not keep files in hashRing, the value itself is IPaddress, not NODE:IP_Address
					string nodeIP = hashRing->getValue(nodePosition);
					//cout << "nodeIP " << nodeIP << endl;
					cout << "[DNSANS] " << "we will put sdfsfilename: " << inMsg[2] << " to nodeIP: " << nodeIP;
					cout << " using localfilename: " << inMsg[1] << endl;
					string sendMsg = nodeIP+"::"+inMsg[1]+"::"+inMsg[2]+"::"+inMsg[3]+"::"+inMsg[4];
					this->tcpServent->pendSendMessages.push(sendMsg);
				}
				break;
			}
			case REREPLICATEGET: {
				if (inMsg.size() >= 3) {
					int nodePosition = stoi(inMsg[0]);
					// since we do not keep files in hashRing, the value itself is IPaddress, not NODE:IP_Address
					string nodeIP = hashRing->getValue(nodePosition);
					string sdfsfilename = inMsg[1];
					string remoteLocalfilename = inMsg[2];
					string localfilename = this->localFilelist[sdfsfilename];
					cout << "[REREPLICATEGET] Got a request of sdfsfilename " << sdfsfilename << " to nodeIP " << nodeIP << endl;
					cout << "[REREPLICATEGET] Put localfilename " << localfilename << " to nodeIP " << nodeIP << endl;
					string sendMsg = nodeIP+"::"+localfilename+"::"+sdfsfilename+"::"+remoteLocalfilename+"::"+"1";
					this->tcpServent->pendSendMessages.push(sendMsg);
				}
				break;
			}
			case REREPLICATE:{
				// Read the answer and send a PUT msg to dest
				if (inMsg.size() >= 2) {
					int nodePosition = stoi(inMsg[0]);
					// since we do not keep files in hashRing, the value itself is IPaddress, not NODE:IP_Address
					string nodeIP = hashRing->getValue(nodePosition);
					string sdfsfilename = inMsg[1];
					string localfilename = this->localFilelist[sdfsfilename];
					cout << "[REREPLICATE] Got a request of sdfsfilename " << sdfsfilename << " to nodeIP " << nodeIP << endl;
					cout << "[REREPLICATE] Put localfilename " << localfilename << " to nodeIP " << nodeIP << endl;
					string sendMsg = nodeIP+"::"+localfilename+"::"+sdfsfilename+"::"+"::"+"1";
					this->tcpServent->pendSendMessages.push(sendMsg);
					//this->tcpServent->sendFile(nodeIP, TCPPORT, localfilename, sdfsfilename, "");
				}
				break;
			}
			case GETNULL: {
				if (inMsg.size() >= 1) {
					cout << "[GETNULL] " << inMsg[0] << endl;
				}
				break;
			}

			case ACK:{
				if (inMsg.size() >= 3) {
					string nodePosition = inMsg[0];
					string sdfsfilename = inMsg[1];
					string localfilename = inMsg[2];
					localFilelist[sdfsfilename] = localfilename;
					Messages outMsg(LEADERACK, this->nodeInformation.ip + "::" + to_string(this->hashRingPosition) + "::" + msg.payload);
					cout << "[ACK] Done replicated sdfsfilename " << sdfsfilename;
					cout << " on node " << nodePosition << ", and ACK back to the leader" << endl;
					this->tcpServent->sendMessage(leaderIP, TCPPORT, outMsg.toString());
				}

				break;
			}
			case LEADERACK:{
				if(isLeader){
					if(inMsg.size() >= 4){
						string inMsgIP = inMsg[0];
						int inMsgnodePosition = stoi(inMsg[1]);
						int nodePosition = stoi(inMsg[2]);
						string sdfsfilename = inMsg[3];
						string replicatedNodeIP = hashRing->getValue(nodePosition);

						cout << "[LEADERACK] Got ACK inMsgIP: " << inMsgIP << " sdfsfilename: " << sdfsfilename << " done on " << replicatedNodeIP << endl;
						string closestNodeIP = "";

						// update fileList
						updateFileList(sdfsfilename, inMsgnodePosition);
						updateFileList(sdfsfilename, nodePosition);

						vector<int> temp;
						cout << "pendingRequests: ";
						if (get<0>(pendingRequests[sdfsfilename]) == nodePosition) {
							closestNodeIP = hashRing->getValue(get<0>(pendingRequests[sdfsfilename]));
							temp.push_back(-1);
						} else {
							temp.push_back(get<0>(pendingRequests[sdfsfilename]));
						}
						cout << temp[0] << " (sent: " << get<0>(pendingRequestSent[sdfsfilename]);
						cout << ", from " << get<0>(pendingSenderRequests[sdfsfilename]) << "), ";
						if (get<1>(pendingRequests[sdfsfilename]) == nodePosition) {
							temp.push_back(-1);
						} else {
							temp.push_back(get<1>(pendingRequests[sdfsfilename]));
						}
						cout << temp[1] << " (sent: " << get<1>(pendingRequestSent[sdfsfilename]);
						cout << ", from " << get<1>(pendingSenderRequests[sdfsfilename]) << "), ";
						if (get<2>(pendingRequests[sdfsfilename]) == nodePosition) {
							temp.push_back(-1);
						} else {
							temp.push_back(get<2>(pendingRequests[sdfsfilename]));
						}
						cout << temp[2] << " (sent:" << get<2>(pendingRequestSent[sdfsfilename]);
						cout << ", from " << get<2>(pendingSenderRequests[sdfsfilename]) << ")" << endl;
						pendingRequests[sdfsfilename] = tuple<int, int, int>(temp[0], temp[1], temp[2]);

						if(get<1>(pendingRequests[sdfsfilename]) == -1 && get<2>(pendingRequests[sdfsfilename])== -1){
							pendingRequests.erase(sdfsfilename);
							pendingRequestSent.erase(sdfsfilename);
							pendingSenderRequests.erase(sdfsfilename);
							cout << "[LEADERACK] 3 or more Replicated files are done" << endl;
							isBlackout = false;
							break;
						}
						if((get<1>(pendingRequests[sdfsfilename])!=-1) && (!get<1>(pendingRequestSent[sdfsfilename]))){
							Messages outMsg(REREPLICATE, to_string(get<1>(pendingRequests[sdfsfilename])) + "::" + sdfsfilename);
							// cout << "Sending out rereplicate to " << inMsgIP << "with message " << outMsg.toString() << endl;
							cout << "[LEADERACK] Ask node incoming " << inMsgIP << " to replicate on pos ";
							cout << to_string(get<1>(pendingRequests[sdfsfilename])) << endl;
							this->tcpServent->sendMessage(inMsgIP, TCPPORT, outMsg.toString());
							pendingRequestSent[sdfsfilename] = tuple<int, int, int>(get<0>(pendingRequestSent[sdfsfilename]), true, get<2>(pendingRequestSent[sdfsfilename]));
							pendingSenderRequests[sdfsfilename] = tuple<string, string, string>(get<0>(pendingSenderRequests[sdfsfilename]), inMsgIP, get<2>(pendingSenderRequests[sdfsfilename]));
						}
						if((get<2>(pendingRequests[sdfsfilename]) != -1) && (!get<2>(pendingRequestSent[sdfsfilename]))){
							Messages outMsg(REREPLICATE, to_string(get<2>(pendingRequests[sdfsfilename])) + "::" + sdfsfilename);
							// cout << "Sending out rereplicate to " << closestNodeIP << "with message " << outMsg.toString() << endl;
							cout << "[LEADERACK] Ask node closest " << closestNodeIP << " to replicate on pos ";
							cout << to_string(get<2>(pendingRequests[sdfsfilename])) << endl;
							this->tcpServent->sendMessage(closestNodeIP, TCPPORT, outMsg.toString());
							pendingRequestSent[sdfsfilename] = tuple<int, int, int>(get<0>(pendingRequestSent[sdfsfilename]), get<1>(pendingRequestSent[sdfsfilename]), true);
							pendingSenderRequests[sdfsfilename] = tuple<string, string, string>(get<0>(pendingSenderRequests[sdfsfilename]), get<1>(pendingSenderRequests[sdfsfilename]), inMsgIP);
						}
					}
				}
				break;
			}
			default:
				break;
		}
		qCopy.pop();
	}
}

void Node::listLocalFiles(){
	cout << "sdfsfilename ---> localfilename" << endl;
	for (auto& element: localFilelist) {
		cout << element.first << " ---> " << element.second << endl;
	}
}

void Node::listSDFSFileList(string sdfsfilename) {
	bool found = false;
	vector<int> foundPositions;
	for (auto& element: fileList) {
		if(element.first.compare(sdfsfilename)==0) { // found sdfsfilename
			found = true;
			foundPositions = element.second;
			break;
		}
	}
	if (found) {
		cout << "sdfsfilename " << sdfsfilename << " is stored at..." << endl;
		if (foundPositions.size() > 0) {
			for (uint i=0; i<foundPositions.size(); i++) {
				string storedIP = hashRing->getValue(foundPositions[i]);
				cout << storedIP << " at " << foundPositions[i] << endl;
			}
		} else { cout << "=== Current list is empty ===" << endl; }
	} else {
		cout << "sdfsfilename " << sdfsfilename << " is not existed" << endl;
	}
}

/*
 * Leader sends out fileList in the following string format:
 * first 2 bytes are filename len, FILENAME msg type, filename itself,
 * 2 bytes for the number of positions the file has, FILEPOSITION msg type,
 * and a string of a commas seperated list of positions following that, ending in null byte.
 * All files are encapsulated in this way and joined to make one string
*/
string Node::encapsulateFileList()
{
	string enMeg = "";
	if (checkLeaderExist() && isLeader) {
		for (auto& element: fileList) {
			string positions = "";
			string sdfsfilename = element.first;

			for (uint i=0; i<element.second.size(); i++) {
				positions += to_string(element.second[i]);
				if (i != element.second.size()-1) {
					positions += ",";
				}
			}
			//cout << "sdfsfilename " << sdfsfilename << endl;
			//cout << "positions " << positions << endl;
			string size = to_string(get<0>(fileSizes[sdfsfilename])) + "," + to_string(get<1>(fileSizes[sdfsfilename]));
			char *cstr = new char[sdfsfilename.length()+positions.length()+size.length()+3+3+3+1];
			size_t len = sdfsfilename.length()+3;
			int index = 0;
			cstr[index++] = len & 0xff;
			cstr[index] = (len >> 8) & 0xff;
			if (cstr[index] == 0) { // avoid null
				cstr[index] = 0xff;
			}
			index++;
			//printf("cstr[0] %x, cstr[1] %x\n", cstr[0], cstr[1]);
			cstr[index++] = FILENAME;
			for (uint i=0; i<sdfsfilename.length(); i++) {
				cstr[index+i] = sdfsfilename.c_str()[i];
			}
			index += sdfsfilename.length();
			size_t len2 = positions.length()+3;
			cstr[index++] = len2 & 0xff;
			cstr[index] = (len2 >> 8) & 0xff;
			if (cstr[index] == 0) { // avoid null
				cstr[index] = 0xff;
			}
			index++;
			//printf("cstr[3] %x, cstr[4] %x\n", cstr[0], cstr[1]);
			cstr[index++] = FILEPOSITIONS;
			//printf("cstr[%lu] %d\n", sdfsfilename.length()+2, cstr[sdfsfilename.length()+2]);
			for (uint i=0; i<positions.length(); i++) {
				cstr[index+i] = positions.c_str()[i];
			}
			index += positions.length();
			size_t len3 = size.length()+3;
			cstr[index++] = len3 & 0xff;
			cstr[index] = (len3 >> 8) & 0xff;
			if (cstr[index] == 0) { // avoid null
				cstr[index] = 0xff;
			}
			index++;
			cstr[index++] = FILESIZE;
			for (uint i=0; i<size.length(); i++) {
				cstr[index+i] = size.c_str()[i];
			}
			index += size.length();
			cstr[index] = '\0';
			//printf("cstrFile %s\n", cstr);
			string enMegFile(cstr);
			//cout << "enMegFile " << enMegFile << endl;
			enMeg += enMegFile;
		}
		//cout << "encapsulateFileList " << enMeg << endl;
	}
	return enMeg;
}

//(len, PayloadType, message, \0) encoding where len is 2 bytes.
string Node::encapsulateMessage(map<PayloadType,string> payloads)
{
	string enMeg = "";
	//cout << "payloads.size " << payloads.size() << endl;
	for (auto& element: payloads) {
		PayloadType type = element.first;
		string message = element.second;
		//cout << "message " << message << endl;
		//cout << "message.length " << message.length() << endl;
		//cout << "type " << type << endl;
		char *cstr = new char[message.length()+4];
		size_t len = message.length()+3;
		cstr[0] = len & 0xff;
		cstr[1] = (len >> 8) & 0xff;
		if (cstr[1] == 0) { // avoid null
			cstr[1] = 0xff;
		}
		//printf("cstr[0] %x, cstr[1] %x\n", cstr[0], cstr[1]);
		cstr[2] = type;
		//printf("cstr[2] %x\n", cstr[2]);
		for (uint i=0; i<message.length(); i++) {
			cstr[i+3] = message.c_str()[i];
		}
		cstr[message.length()+3] = '\0';
		//printf("cstrMsg %s\n", cstr);
		string enMegPart(cstr);
		//cout << "enMegPart " << enMegPart << endl;
		enMeg += enMegPart;
	}
	//cout << "encapsulateMessage " << enMeg << endl;
	return enMeg;
}

void Node::decapsulateFileList(string payload)
{
	int size = payload.length();
	uint pos = 0;
	fileList.clear();
	fileSizes.clear();
	string lastFilename = "";
	while (size > 0) {
		size_t length;
		if ((payload.c_str()[1+pos] & 0xff) == 0xff) {
			length = 0;
		} else {
			length = (payload.c_str()[1+pos]) & 0xff;
			length = length << 8;
		}
		length += (payload.c_str()[0+pos]) & 0xff;
		PayloadType type = static_cast<PayloadType>(payload.c_str()[2+pos]);
		//printf(" len %lu, type %d\n", length, type);
		char cstr[length];
		bzero(cstr, sizeof(cstr));
		for (uint i=3; i<length; i++) {
			cstr[i-3] = payload.c_str()[pos+i];
		}
		string deMegPart(cstr);
		switch (type) {
			case FILENAME: {
				//cout << "FILENAME " << deMegPart << endl;
				lastFilename = deMegPart;
				break;
			}
			case FILEPOSITIONS: {
				//cout << "FILEPOSITIONS " << deMegPart << endl;
				vector<string> temp = splitString(deMegPart, ",");
				vector<int> positions;
				for (uint i=0; i<temp.size(); i++) {
					if (temp[i].compare("")!=0) {
						positions.push_back(stoi(temp[i]));
					}
				}
				fileList[lastFilename] = positions;
				break;
			}
			case FILESIZE: {
				vector<string> temp = splitString(deMegPart, ",");
				fileSizes[lastFilename] = make_tuple(stol(temp[0]),stoi(temp[1]));
				break;
			}
			default:
				break;
		}
		size -= length;
		pos += length;
	}

	// check with local file list
	if (!isLeader) {
		vector<string> fileToDelete;
		for (auto& element: localFilelist) {
			//cout << "sdfsfilename " << element.first << endl;
			if (fileList[element.first].size() == 0) {
				fileToDelete.push_back(element.first);
			}
		}
		for (uint i=0; i<fileToDelete.size(); i++) {
			localFilelist.erase(fileToDelete[i]);
			cout << "[DELETE] sdfsfilename " << fileToDelete[i] << endl;
		}
	}
}

string Node::decapsulateMessage(string payload)
{
	int size = payload.length();
	uint pos = 0;
	//cout << "payload " << payload << endl;
	//cout << "size " << size << endl;
	string deMeg = "";
	while (size > 0) {
		size_t length;
		if ((payload.c_str()[1+pos] & 0xff) == 0xff) {
			length = 0;
		} else {
			length = (payload.c_str()[1+pos]) & 0xff;
			length = length << 8;
		}
		length += (payload.c_str()[0+pos] & 0xff);
		//printf("lengthMeg %x %x %lu\n", payload.c_str()[0+pos], payload.c_str()[1+pos], length);

		PayloadType type = static_cast<PayloadType>(payload.c_str()[2+pos]);
		//printf(" len %lu, type %d\n", length, type);
		char cstr[length];
		bzero(cstr, sizeof(cstr));
		for (uint i=3; i<length; i++) {
			cstr[i-3] = payload.c_str()[pos+i];
		}
		//printf("cstr %s\n", cstr);
		string deMegPart(cstr);
		//cout << "deMegPart " << deMegPart << endl;
		if (type == REGULAR) {
			deMeg = deMegPart;
		} else if (type == FILEPAIR) {
			if (checkLeaderExist() && !isLeader) {
				//cout << "FILEPAIR " << deMegPart << endl;
				decapsulateFileList(deMegPart);
			}
		}
		//cout << "size1 " << size << endl;
		size -= length;
		pos += length;
		//cout << "size2 " << size << endl;
	}
	//cout << "deMeg " << deMeg << endl;
	return deMeg;
}

// piggyback fileList in heartbeat
string Node::populateSDFSFileList(MessageType type, string mem_list_to_send)
{
	Messages msg(type, mem_list_to_send);
	//cout << "populateSDFSFileList " << msg.toString() << endl;
	map<PayloadType,string> payloads;
	payloads[REGULAR] = msg.toString();
	if (isLeader) { // Only the leader includes the fileList
		payloads[FILEPAIR] = encapsulateFileList();
	}
	string enMeg = encapsulateMessage(payloads);
	return enMeg;
}