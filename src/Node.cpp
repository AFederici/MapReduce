#include "../inc/Node.h"


//add another function to Node class for failure detection
//call function before sender (heartbeat) after listenForHeartbeat

Node::Node()
{
	// create a udp object
	udpServent = new UdpSocket();
	tcpServent = new TcpSocket();
	hashRing = new HashRing();
	localTimestamp = 0;
	heartbeatCounter = 0;
	//time(&startTimestamp);
	// byteSent = 0;
	// byteReceived = 0;
	runningMode = ALL2ALL;
	activeRunning = false;
	prepareToSwitch = false;
	logWriter = new Logger(LOGGING_FILE_NAME);
	leaderPosition = -1;
	proposedTime = 0;
	electedTime = 0;
	joinTimestamp = "";
	possibleSuccessorIP = "";
	leaderIP = "";
	leaderPort = ""; 
	isBlackout = true;
}

Node::Node(ModeType mode)
{
	// create a udp object
	udpServent = new UdpSocket();
	tcpServent = new TcpSocket();
	hashRing = new HashRing();
	localTimestamp = 0;
	heartbeatCounter = 0;
	//time(&startTimestamp);
	// byteSent = 0;
	// byteReceived = 0;
	runningMode = mode;
	activeRunning = false;
	prepareToSwitch = false;
	logWriter = new Logger(LOGGING_FILE_NAME);
	leaderPosition = -1;
	proposedTime = 0;
	electedTime = 0;
	joinTimestamp = "";
	possibleSuccessorIP = "";
	leaderIP = "";
	leaderPort = "";
	isBlackout = true;
}

void Node::startActive()
{
	membershipList.clear();
	restartElection();
	// inserting its own into the list
	time(&startTimestamp);
	string startTime = ctime(&startTimestamp);
	startTime = startTime.substr(0, startTime.find("\n"));
	tuple<string,string,string> mapKey(nodeInformation.ip, nodeInformation.port, startTime);
	tuple<int, int, int> valueTuple(nodeInformation.heartbeatCounter, nodeInformation.timestamp, 0);
	membershipList[mapKey] = valueTuple;
			
	debugMembershipList();
	joinTimestamp = startTime; // for hashRing
	getPositionOnHashring(); // update its hashRingPosition
}

void Node::computeAndPrintBW(double diff)
{
#ifdef LOG_VERBOSE
	cout << "total " << udpServent->byteSent << " bytes sent" << endl;
	cout << "total " << udpServent->byteReceived << " bytes received" << endl;
	printf("elasped time is %.2f s\n", diff);
#endif
	if (diff > 0) {
		double bandwidth = udpServent->byteSent/diff;
		string message = "["+to_string(this->localTimestamp)+"] B/W usage: "+to_string(bandwidth)+" bytes/s";
#ifdef LOG_VERBOSE
		printf("%s\n", message.c_str());
#endif
		this->logWriter->printTheLog(BANDWIDTH, message);
	}
}

void Node::updateNodeHeartbeatAndTime()
{
	string startTime = ctime(&startTimestamp);
	startTime = startTime.substr(0, startTime.find("\n"));
	tuple<string, string, string> keyTuple(nodeInformation.ip, nodeInformation.port,startTime);
	tuple<int, int, int> valueTuple(heartbeatCounter, localTimestamp, 0);
	this->membershipList[keyTuple] = valueTuple;
}

string Node::populateMembershipMessage()
{
	//The string we send will be seperated line by line --> IP,PORT,HeartbeatCounter,FailFlag
	string mem_list_to_send = "";
	//Assume destination already exists in the membership list of this node, just a normal heartbeat
	switch (this->runningMode)
	{
		case GOSSIP:
			for (auto& element: this->membershipList) {
				tuple<string, string, string> keyTuple = element.first;
				tuple<int, int, int> valueTuple = element.second;
				mem_list_to_send += get<0>(keyTuple) + "," + get<1>(keyTuple) + "," + get<2>(keyTuple) + ","; 
				mem_list_to_send += to_string(get<0>(valueTuple)) + "," + to_string(get<2>(valueTuple)) + "\n";
			}
			break;
		
		default:
			string startTime = ctime(&startTimestamp);
			startTime = startTime.substr(0, startTime.find("\n"));
			mem_list_to_send += nodeInformation.ip + "," + nodeInformation.port + "," + startTime + ",";
			mem_list_to_send += to_string(heartbeatCounter) + "," + to_string(0) + "\n";
			break;
	}
	return mem_list_to_send;
}

string Node::populateIntroducerMembershipMessage(){
	string mem_list_to_send = "";
	for (auto& element: this->membershipList) {
		tuple<string, string, string> keyTuple = element.first;
		tuple<int, int, int> valueTuple = element.second;
		mem_list_to_send += get<0>(keyTuple) + "," + get<1>(keyTuple) + "," + get<2>(keyTuple) + ","; 
		mem_list_to_send += to_string(get<0>(valueTuple)) + "," + to_string(get<2>(valueTuple)) + "\n";
	}
	return mem_list_to_send;
}

/**
 * 
 * HeartbeatToNode: Sends a string version of the membership list to the receiving node. The receiving node will convert the string to
 * a <string, long> map where the key is the Addresss (IP + PORT) and value is the heartbeat counter. We then compare the Member.
 * 
 **/
int Node::heartbeatToNode()
{
	// 3. prepare to send heartbeating, and 
	string mem_list_to_send = populateMembershipMessage();
	vector<tuple<string,string,string>> targetNodes = getRandomNodesToGossipTo();
	
	//Now we have messages ready to send, need to invoke UDP client to send 
#ifdef LOG_VERBOSE
	cout << "pick " << targetNodes.size() << " of " << this->membershipList.size()-1;
	cout << " members" << endl;
#endif
	
	// 4. do gossiping
	for (uint i=0; i<targetNodes.size(); i++) {
		//cout << targetNodes[i].first << "/" << targetNodes[i].second << endl;
		Member destination(get<0>(targetNodes[i]), get<1>(targetNodes[i]));

		string message = "["+to_string(this->localTimestamp)+"] node "+destination.ip+"/"+destination.port+"/"+get<2>(targetNodes[i]);
#ifdef LOG_VERBOSE
		cout << "[Gossip]" << message.c_str() << endl;
#endif
		this->logWriter->printTheLog(GOSSIPTO, message);

		//cout << mem_list_to_send.size() << " Bytes sent..." << endl;
		// byteSent += mem_list_to_send.size();
		if (isLeader) {
			//Messages msg(LEADERHEARTBEAT, mem_list_to_send);
			if (isBlackout) {
				string msg = populateSDFSFileList(LEADERPENDING, mem_list_to_send);
				udpServent->sendMessage(destination.ip, destination.port, msg);
			} else {
				string msg = populateSDFSFileList(LEADERHEARTBEAT, mem_list_to_send);
				udpServent->sendMessage(destination.ip, destination.port, msg);
			}
		} else {
			//Messages msg(HEARTBEAT, mem_list_to_send);
			string msg = populateSDFSFileList(HEARTBEAT, mem_list_to_send);
			udpServent->sendMessage(destination.ip, destination.port, msg);
		}
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
			// do not check itself
#ifdef LOG_VERBOSE
			cout << "skip it" << endl;
#endif	
			continue;
		}
		if(get<2>(valueTuple) == 0){
			if(localTimestamp - get<1>(valueTuple) > T_timeout){
				//cout << "Got " << get<0>(keyTuple) << "/" << get<1>(keyTuple) << "/" << get<2>(keyTuple) << endl;
				//cout << "local time " << localTimestamp << " vs. " << get<1>(valueTuple) << endl;
				get<1>(this->membershipList[keyTuple]) = localTimestamp;
				get<2>(this->membershipList[keyTuple]) = 1;
				
				string message = "["+to_string(this->localTimestamp)+"] node "+get<0>(keyTuple)+"/"+get<1>(keyTuple)+"/"+get<2>(keyTuple)+": Local Failure";
				cout << "[FAIL]" << message.c_str() << endl;
				this->logWriter->printTheLog(FAIL, message);
				if(isLeader){
					// clearn up fileList
					Member deletedNode(get<0>(keyTuple), get<1>(keyTuple));
					int deletedNodePostion = hashingId(deletedNode, get<2>(keyTuple));
					hashRing->removeNode(deletedNodePostion);
					for (auto& element: fileList) {
						vector<int> newEntry;
						for(unsigned int i = 0; i < element.second.size(); i++){
							if(element.second[i] != deletedNodePostion){
								newEntry.push_back(element.second[i]);
							}
						}
						fileList[element.first] = newEntry;
					}

					// chech if the failure is the sender in pending requests
					for (auto& senders: pendingSenderRequests) {
						string sdfsfilename = senders.first;
						tuple<string,string,string> sender = senders.second;
						if ((get<0>(keyTuple).compare(get<0>(sender))==0) &&
							get<0>(pendingRequestSent[sdfsfilename]) &&
							(get<0>(pendingRequests[sdfsfilename])!=-1)) {
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
							(get<1>(pendingRequests[sdfsfilename])!=-1)) {
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
							(get<2>(pendingRequests[sdfsfilename])!=-1)) {
							// it sent out, but replicates are failed
							// restart again
							cout << "[PUT/REREPLICATE] The sender " << get<0>(keyTuple) << " failed, try again" << endl;
							pendingRequests.erase(sdfsfilename);
						}
					}
					
				}
				
			}
		}
		else{
			if(localTimestamp - get<1>(valueTuple) > T_cleanup){
				// core dumped happened here; bug fix
				auto iter = this->membershipList.find(keyTuple);
				if (iter != this->membershipList.end()) {
					//cout << "Got " << get<0>(iter->first) << "/" << get<1>(iter->first) << "/" << get<2>(iter->first);
					//cout << " with " << to_string(get<0>(iter->second)) << "/";
					//cout << to_string(get<1>(iter->second)) << "/";
					//cout << to_string(get<2>(iter->second)) << endl;
					//cout << this->membershipList[keyTuple]
					//this->membershipList.erase(iter);
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

			//this->debugMembershipList();
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
	//Messages msg(JOIN, mem_list_to_send);
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
	//Messages msg(SWREQ, message);
	string msg = populateSDFSFileList(SWREQ, message);
	for(auto& element: this->membershipList) {
		tuple<string,string,string> keyTuple = element.first;
		//tuple<int, int, int> valueTuple = element.second;
		cout << "[SWITCH] sent a request to " << get<0>(keyTuple) << "/" << get<1>(keyTuple) << endl;
		udpServent->sendMessage(get<0>(keyTuple), get<1>(keyTuple), msg);
	}
	return 0;
}

int Node::SwitchMyMode() 
{
	// wait for a while
	sleep(T_switch);
	// empty all messages
	udpServent->qMessages = queue<string>();
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
	// finishing up
	prepareToSwitch = false;
	return 0;
}
 
int Node::listenToHeartbeats() 
{
	//look in queue for any strings --> if non empty, we have received a message and need to check the membership list

	// 1. deepcopy and handle queue
	queue<string> qCopy(udpServent->qMessages);
	udpServent->qMessages = queue<string>();

	int size = qCopy.size();
	//cout << "Got " << size << " messages in the queue" << endl;
	//cout << "checking queue size " << nodeOwn->udpServent->qMessages.size() << endl;

	// 2. merge membership list
	for (int j = 0; j < size; j++) {
		//cout << qCopy.front() << endl;
		readMessage(qCopy.front());

		// Volunteerily leave
		if(this->activeRunning == false){
			return 0;
		}
		// byteReceived += qCopy.front().size();
		qCopy.pop();
	}

	return 0;
}

void Node::debugMembershipList()
{
	cout << "Membership list [" << this->membershipList.size() << "]:" << endl;
	if (isLeader) {
		cout << "[T]   IP/Port/JoinedTime:Heartbeat/LocalTimestamp/FailFlag" << endl;
	} else {
		cout << "[T] IP/Port/JoinedTime:Heartbeat/LocalTimestamp/FailFlag" << endl;
	}
	string message = "";
	
	for (auto& element: this->membershipList) {
		tuple<string,string,string> keyTuple = element.first;
		tuple<int, int, int> valueTuple = element.second;

		if (nodeInformation.ip.compare(get<0>(keyTuple))==0) { // Myself
			if (isLeader) {
				message += "[L/M] ";
			} else {
				message += "[M] ";
			}
		} else if (leaderIP.compare(get<0>(keyTuple))==0) {
			message += "[L] ";
		} else {
			if (isLeader) {
				message += "      ";
			} else {
				message += "    ";
			}
		}

		message += get<0>(keyTuple)+"/"+get<1>(keyTuple)+"/"+get<2>(keyTuple);
		message += ": "+to_string(get<0>(valueTuple))+"/"+to_string(get<1>(valueTuple))+"/"+to_string(get<2>(valueTuple))+"\n";
	}
	cout << message.c_str() << endl;
	this->logWriter->printTheLog(MEMBERS, message);
}

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
		if (membershipListEntry.size() != 5) {
			// circumvent craching
			continue;
		}

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
			
			// do not check itself heartbeat
#ifdef LOG_VERBOSE
			cout << "skip it" << endl;
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
								// get<2>(this->membershipList[mapKey]) = failFlag;
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
			} else {
				// do nothing
			}	
		}
	}

	// If membership list changed in all-to-all, full membership list will be sent
	if(changed && this->runningMode == ALL2ALL){
		string mem_list_to_send = populateIntroducerMembershipMessage();
		vector<tuple<string,string,string>> targetNodes = getRandomNodesToGossipTo();

		for (uint i=0; i<targetNodes.size(); i++) {
			Member destination(get<0>(targetNodes[i]), get<1>(targetNodes[i]));

			string message = "["+to_string(this->localTimestamp)+"] node "+destination.ip+"/"+destination.port+"/"+get<2>(targetNodes[i]);
#ifdef LOG_VERBOSE
			cout << "[Gossip]" << message.c_str() << endl;
#endif
			this->logWriter->printTheLog(GOSSIPTO, message);

			if (isLeader) {
				//Messages msg(LEADERHEARTBEAT, mem_list_to_send);
				if (isBlackout) {
					string msg = populateSDFSFileList(LEADERPENDING, mem_list_to_send);
					udpServent->sendMessage(destination.ip, destination.port, msg);
				} else {
					string msg = populateSDFSFileList(LEADERHEARTBEAT, mem_list_to_send);
					udpServent->sendMessage(destination.ip, destination.port, msg);
				}
			} else {
				//Messages msg(HEARTBEAT, mem_list_to_send);
				string msg = populateSDFSFileList(HEARTBEAT, mem_list_to_send);
				udpServent->sendMessage(destination.ip, destination.port, msg);
			}

		}		
	}
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
 * 
 * Found help on how to do string processing part of this at https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
 */
void Node::readMessage(string message){
	
	// decapsulate with specific messages
	//cout << "readMessage " << message << endl;
	string deMeg = decapsulateMessage(message);
	bool pending = true;
	//cout << "readMessage deMeg " << deMeg << endl;
	
	Messages msg(deMeg);
	switch (msg.type) {
		case LEADERHEARTBEAT: // Note: not for Gossip-style, only All-to-All
			//cout << "LEADERHEARTBEAT: " << msg.payload << endl;
			pending = false;
		case LEADERPENDING:
			setUpLeader(msg.payload, pending);
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
					//Messages response(JOINREJECT, "");
					string response = populateSDFSFileList(JOINREJECT, "");
					udpServent->sendMessage(fields[0], fields[1], response);
				} else {
					string introducerMembershipList;
					introducerMembershipList = populateIntroducerMembershipMessage();
					//Messages response(JOINRESPONSE, introducerMembershipList);
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
	//debugMembershipList();	
}

vector<string> Node::splitString(string s, string delimiter){
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

int Node::hashingId(Member nodeMember, string joinTime)
{
	string toBeHashed = "NODE::" + nodeMember.ip + "::" + nodeMember.port + "::" + joinTime;
	int ringPosition = hash<string>{}(toBeHashed) % HASHMODULO;
	return ringPosition;
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

void Node::leaderCreateHashRing()
{
	// The leader or notes creates hashRing
	hashRing->clear();
	for (auto& element: this->membershipList) { // update hashRing
		tuple<string, string, string> keyTuple = element.first;
		Member member(get<0>(keyTuple), get<1>(keyTuple));
		int pos = hashingId(member, get<2>(keyTuple));
		//hashRing->addNode("NODE::"+get<0>(keyTuple), pos); // since we don't store file, remove NODE
		hashRing->addNode(get<0>(keyTuple), pos);
	}
	//hashRing->debugHashRing();
}

void Node::proposeToBeLeader()
{
	// Start election
	Messages msg(ELECTION, to_string(hashRingPosition));
	cout << "[ELECTION] Propose to be leader, send to " << possibleSuccessorIP << endl;
 	tcpServent->sendMessage(possibleSuccessorIP, TCPPORT, msg.toString());
}

void Node::processElection(Messages messages)
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

void Node::processTcpMessages()
{
	queue<string> qCopy(tcpServent->qMessages);
	tcpServent->qMessages = queue<string>();

	int size = qCopy.size();
	//cout << "Got " << size << " TCP messages" << endl;

	for (int j=0; j<size; j++) {
		//cout << qCopy.front() << endl;
		Messages msg(qCopy.front());
		//cout << "Has " << msg.type << " with " << msg.payload << endl;
		switch (msg.type) {
			case ELECTION:
			case ELECTIONACK: {
				processElection(msg);
				break;
			}
			default:
				break;
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

//Can only be called when we are the leader node, since we are going to be calling REREPLICATE here, and checking the global file list. 
//Called in ProcessRegMessages before we do anything else, since we want to check the global file list consistency before we process the other messages
//In the Queue. 
void Node::checkFileListConsistency(){
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
					if(get<0>(request) == 0 && get<1>(request) == 0 && get<2>(request) == 0){
						pendingRequests[element.first] = tuple<int, int, int>(-1, -1, nodesToCheck[i]);
						pendingRequestSent[element.first] = tuple<int, int, int>(true, true, true);
						tcpServent->sendMessage(leaderIP, TCPPORT, outMsg.toString());
						break;
					}
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

bool Node::isInVector(vector<int> v, int i){
	for(int element: v){
		if(element == i){
			return true;
		}
	}
	return false;
}


void Node::processRegMessages()
{
	//Before we do anything here, we should have the leader check to see if the file list is consistent or not. 
	//We do this by:
	//1. Checking the files in the filelist, making sure each one has 4 entries. If not, then we need to rereplicate.
	// We can initiate a PUT, put pending request, setting as -1, -1, and then last one as target node that we want to replicate to (new node to replace the one that failed)
	if(isLeader){
		checkFileListConsistency();
	}
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
			if(k == msgSplit.size() - 1){
				payload += msgSplit[k];
			} else {
				payload += msgSplit[k] + "::";
			}
		}
		MessageType msgType = static_cast<MessageType>(stoi(msgSplit[0]));
		Messages msg(msgType, payload);
		// cout << "Has " << msg.type << " with " << msg.payload << endl;
		switch (msg.type) {
			case PUTACK: {
				vector<string> inMsg = splitString(msg.payload, "::");
				if(inMsg.size() >= 4){
					string inMsgIP = inMsg[0];
					string sdfsfilename = inMsg[1];
					string localfilename = inMsg[2];
					string remoteLocalname = inMsg[3];

					cout << "[PUTACK] " << "inMsgIP: " << inMsgIP << " sdfsfilename: " << sdfsfilename << " localfilename: " << localfilename << endl;
					
					localFilelist[sdfsfilename] = localfilename;
					Messages outMsg(ACK, to_string(this->hashRingPosition)+"::"+sdfsfilename+"::"+remoteLocalname);
					this->tcpServent->sendMessage(inMsgIP, TCPPORT, outMsg.toString());
				}
				break;
			}
			case DELETE: {
				if (isLeader) {
					vector<string> inMsg = splitString(msg.payload, "::");
					if(inMsg.size() >= 2){
						string inMsgIP = inMsg[0];
						string sdfsfilename = inMsg[1];

						cout << "[DELETE] " << "inMsgIP: " << inMsgIP << " sdfsfilename: " << sdfsfilename << endl;
						localFilelist.erase(sdfsfilename);
						fileList.erase(sdfsfilename);
						// This is TCP, so we don't need to ACK
					}
				}
				break;
			}
			case DNSGET: {
				if(isLeader){
					// Do replicating to the node
					//isBlackout = true;
					vector<string> inMsg = splitString(msg.payload, "::");
					cout << "msg.payload " << msg.payload << endl;
					if(inMsg.size() >= 4){
						string inMsgIP = inMsg[0];
						int nodePosition = stoi(inMsg[1]);
						int selectedNodePosition = nodePosition;
						string sdfsfilename = inMsg[2];
						string localfilename = inMsg[3];
						cout << "[DNSGET] Got " << "inMsgIP: " << inMsgIP << ", sdfsfilename: " << sdfsfilename << ", localfilename: " << localfilename << endl;
						vector<int> positions = fileList[sdfsfilename];
						if (positions.size() == 0) {
							// the file is not available
							cout << "[DNSGET] sdfsfilename " << sdfsfilename << " is not available" << endl;
							fileList.erase(sdfsfilename);
							Messages outMsg(GETNULL, sdfsfilename+": the file is not available::");
							this->tcpServent->sendMessage(inMsgIP, TCPPORT, outMsg.toString());
							//isBlackout = false;
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
					vector<string> inMsg = splitString(msg.payload, "::");
					if(inMsg.size() >= 4){
						string inMsgIP = inMsg[0];
						int nodePosition = stoi(inMsg[1]);
						string sdfsfilename = inMsg[2];
						string localfilename = inMsg[3];

						cout << "[DNS] Got " << "inMsgIP: " << inMsgIP << ", sdfsfilename: " << sdfsfilename;
						cout << ", localfilename: " << localfilename << ", pos: " << nodePosition << endl;
						//this->localFilelist[sdfsfilename] = localfilename;
						// update fileList, client itself is one of the replicas
						updateFileList(sdfsfilename, nodePosition);
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
						Messages outMsg(DNSANS, to_string(closestNode) + "::" + localfilename + "::" + sdfsfilename);
						this->tcpServent->sendMessage(inMsgIP, TCPPORT, outMsg.toString());
					}
				}

				break;
			}
			case DNSANS:{
				// Read the answer and send a PUT msg to dest
				vector<string> inMsg = splitString(msg.payload, "::");
				if(inMsg.size() >= 3){
					int nodePosition = stoi(inMsg[0]);
					// since we do not keep files in hashRing, the value itself is IPaddress, not NODE:IP_Address
					string nodeIP = hashRing->getValue(nodePosition);
					//cout << "nodeIP " << nodeIP << endl;

					cout << "[DNSANS] " << "we will put sdfsfilename: " << inMsg[2] << " to nodeIP: " << nodeIP;
					cout << " using localfilename: " << inMsg[1] << endl;

					string sendMsg = nodeIP+"::"+inMsg[1]+"::"+inMsg[2]+"::";
					this->tcpServent->pendSendMessages.push(sendMsg);
					//this->tcpServent->sendFile(nodeIP, TCPPORT, inMsg[1], inMsg[2], "");
				}
				break;
			}
			case REREPLICATEGET: {
				vector<string> inMsg = splitString(msg.payload, "::");
				if (inMsg.size() >= 3) {
					int nodePosition = stoi(inMsg[0]);
					// since we do not keep files in hashRing, the value itself is IPaddress, not NODE:IP_Address
					string nodeIP = hashRing->getValue(nodePosition);
					string sdfsfilename = inMsg[1];
					string remoteLocalfilename = inMsg[2];
					string localfilename = this->localFilelist[sdfsfilename];
					cout << "[REREPLICATEGET] Got a request of sdfsfilename " << sdfsfilename << " to nodeIP " << nodeIP << endl;
					cout << "[REREPLICATEGET] Put localfilename " << localfilename << " to nodeIP " << nodeIP << endl;
					string sendMsg = nodeIP+"::"+localfilename+"::"+sdfsfilename+"::"+remoteLocalfilename;
					this->tcpServent->pendSendMessages.push(sendMsg);
					//this->tcpServent->sendFile(nodeIP, TCPPORT, localfilename, sdfsfilename, remoteLocalfilename);
				}
				break;
			}
			case REREPLICATE:{
				// Read the answer and send a PUT msg to dest
				vector<string> inMsg = splitString(msg.payload, "::");
				if (inMsg.size() >= 2) {
					int nodePosition = stoi(inMsg[0]);
					// since we do not keep files in hashRing, the value itself is IPaddress, not NODE:IP_Address
					string nodeIP = hashRing->getValue(nodePosition);
					string sdfsfilename = inMsg[1];
					string localfilename = this->localFilelist[sdfsfilename];
					cout << "[REREPLICATE] Got a request of sdfsfilename " << sdfsfilename << " to nodeIP " << nodeIP << endl;
					cout << "[REREPLICATE] Put localfilename " << localfilename << " to nodeIP " << nodeIP << endl;
					string sendMsg = nodeIP+"::"+localfilename+"::"+sdfsfilename+"::";
					this->tcpServent->pendSendMessages.push(sendMsg);
					//this->tcpServent->sendFile(nodeIP, TCPPORT, localfilename, sdfsfilename, "");
				}
				break;
			}
			case GETNULL: {
				vector<string> inMsg = splitString(msg.payload, "::");
				if (inMsg.size() >= 1) {
					cout << "[GETNULL] " << inMsg[0] << endl;
				}
				break;
			}

			case ACK:{
				vector<string> inMsg = splitString(msg.payload, "::");
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
					//TODO: tick the list off
					vector<string> inMsg = splitString(msg.payload, "::");
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

/**
 * Store the given filename in your sdfs filename, discard the original name, and 
 * give it a new name. The hashing will be done based on this sdfs filename. 
 * 
 * Can be called by any node, this one will be called by sender 
 * 
*/
// int Node::putFileSender(string filename, string sdfsfilename){
// 	tcpServent->sendFile(leaderIP, leaderPort, filename);
// 	return 0;

// }

// int Node::putFileMaster(string sdfsfilename){
// 	return 0;
// }

// int Node::putFileReeiver(string sdfsfilename){
// 	return 0;

// }

void Node::listLocalFiles(){
	cout << "sdfsfilename ---> localfilename" << endl;
	for (auto& element: localFilelist) {
		cout << element.first << " ---> " << element.second << endl;
	}
}

void Node::debugSDFSFileList() {
	cout << "sdfsfilename ---> positions,..." << endl;
	for (auto& element: fileList) {
		cout << element.first << " ---> ";
		for (uint i=0; i<element.second.size(); i++) {
			cout << element.second[i];
			if (i == element.second.size()-1) {
				continue;
			} else {
				cout << ", ";
			}
		}
		cout << endl;
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
		if (foundPositions.size() > 0) {
			cout << "sdfsfilename " << sdfsfilename << " is stored at..." << endl;
			cout << "=========" << endl;
			for (uint i=0; i<foundPositions.size(); i++) {
				string storedIP = hashRing->getValue(foundPositions[i]);
				cout << storedIP << " at " << foundPositions[i] << endl;
			}
		} else {
			cout << "sdfsfilename " << sdfsfilename << " is stored at..." << endl;
			cout << "=== Current list is empty ===" << endl;
		}
	} else {
		cout << "sdfsfilename " << sdfsfilename << " is not existed" << endl;
	}
}

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
			char *cstr = new char[sdfsfilename.length()+positions.length()+7];
			size_t len = sdfsfilename.length()+3;
			cstr[0] = len & 0xff;
			cstr[1] = (len >> 8) & 0xff;
			if (cstr[1] == 0) { // avoid null
				cstr[1] = 0xff;
			}
			//printf("cstr[0] %x, cstr[1] %x\n", cstr[0], cstr[1]);
			cstr[2] = FILENAME;
			for (uint i=0; i<sdfsfilename.length(); i++) {
				cstr[i+3] = sdfsfilename.c_str()[i];
			}
			size_t len2 = positions.length()+3;
			cstr[sdfsfilename.length()+3] = len2 & 0xff;
			cstr[sdfsfilename.length()+4] = (len2 >> 8) & 0xff;
			if (cstr[sdfsfilename.length()+4] == 0) { // avoid null
				cstr[sdfsfilename.length()+4] = 0xff;
			}
			//printf("cstr[3] %x, cstr[4] %x\n", cstr[0], cstr[1]);
			cstr[sdfsfilename.length()+5] = FILEPOSITIONS;
			//printf("cstr[%lu] %d\n", sdfsfilename.length()+2, cstr[sdfsfilename.length()+2]);
			for (uint i=0; i<positions.length(); i++) {
				cstr[sdfsfilename.length()+6+i] = positions.c_str()[i];
			}
			cstr[sdfsfilename.length()+positions.length()+6] = '\0';
			//printf("cstrFile %s\n", cstr);
			string enMegFile(cstr);
			//cout << "enMegFile " << enMegFile << endl;
			enMeg += enMegFile;
		}
		//cout << "encapsulateFileList " << enMeg << endl;
	}
	return enMeg;
}

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

void Node::findNodesWithFile(string sdfsfilename){
	/*tuple<int, int, int> nodes = fileList[sdfsfilename];
	cout << hashRing->getValue(get<0>(nodes)) << endl;
	cout << hashRing->getValue(get<1>(nodes)) << endl;
	cout << hashRing->getValue(get<2>(nodes)) << endl;*/
}

