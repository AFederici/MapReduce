#include "../inc/Node.h"

int main(int argc, char *argv[])
{
	pthread_t threads[5];
	int rc;
	Node *node;
	cout << "Mode: " << ALL2ALL << "->All-to-All, ";
	cout << GOSSIP << "->Gossip-style" << endl;
	if (argc < 2) node = new Node();
	else {
		ModeType mode = ALL2ALL;
		if (atoi(argv[1]) == 1) mode = GOSSIP;
		node = new Node(mode);
	}
	cout << "Running mode: " << node->runningMode << endl;
	Member own(getIP(), PORT, node->localTimestamp, node->heartbeatCounter);
	node->nodeInformation = own;
	cout << "[NEW] Starting Node at " << node->nodeInformation.ip << "/";
	cout << node->nodeInformation.port << "..." << endl;
	int *ret;
	string s;
	string cmd;
	bool joined = false;

	// listening server can run first regardless of running time commands
	if ((rc = pthread_create(&threads[0], NULL, runUdpServer, (void *)node->udpServent)) != 0) {
		cout << "Error:unable to create thread," << rc << endl;
		exit(-1);
	}
	if ((rc = pthread_create(&threads[2], NULL, runTcpServer, (void *)node->tcpServent)) != 0) {
		cout << "Error:unable to create thread," << rc << endl;
		exit(-1);
	}
	if ((rc = pthread_create(&threads[4], NULL, runTcpSender, (void *)node->tcpServent)) != 0) {
		cout << "Error:unable to create thread," << rc << endl;
		exit(-1);
	}
	node->localFilelist.clear(); // for testing
	node->fileSizes.clear();
	/*node->localFilelist["sdfsfilename1"] = "localfilename1";
	node->localFilelist["sdfsfilename2"] = "localfilename2";*/

	while(1){
		string s;
		getline (cin, s);
		vector<string> cmdLineInput;
		string delimiter = " ";
		size_t pos_start = 0, pos_end, delim_len = delimiter.length();
		string token;
		while ((pos_end = s.find (delimiter, pos_start)) != string::npos) {
			token = s.substr (pos_start, pos_end - pos_start);
			pos_start = pos_end + delim_len;
			cmdLineInput.push_back (token);
		}
		cmdLineInput.push_back (s.substr (pos_start));
		cmd = cmdLineInput[0];
		// Deal with multiple cmd input
		if(cmd == "join"){
			node->startActive();
			if ((rc = pthread_create(&threads[1], NULL, runSenderThread, (void *)node)) != 0) {
				cout << "Error:unable to create thread," << rc << endl;
				exit(-1);
			}
			joined = true;
		} else if(cmd == "leave" && joined){
				node->activeRunning = false;
				node->membershipList.clear();
				node->restartElection(); // clean up leader info
				pthread_join(threads[1], (void **)&ret);
				string message = "["+to_string(node->localTimestamp)+"] node "+node->nodeInformation.ip+"/"+node->nodeInformation.port+" is left";
				cout << "[LEAVE]" << message.c_str() << endl;
				node->logWriter->printTheLog(LEAVE, message);
				sleep(2); // wait for logging
				joined = false;
		} else if(cmd == "id"){
			cout << "ID: (" << node->nodeInformation.ip << ", " << node->nodeInformation.port << ")" << endl;
		} else if(cmd == "member"){
			debugMembershipList(node);
		} else if(cmd == "switch") {
			if(joined) node->requestSwitchingMode();
		} else if(cmd == "mode") {
			cout << "In " << node->runningMode << " mode" << endl;
		} else if(cmd == "exit"){
			cout << "exiting..." << endl; break;
		} else if (cmd == "put" && joined){ // MP2 op1
			if(cmdLineInput.size() < 3){
				cout << "USAGE: put filename sdfsfilename" << endl;
				continue;
			}
			if (!node->isBlackout) {
				string localfilename = cmdLineInput[1];
				string sdfsfilename = cmdLineInput[2];
				FILE * fp = fopen(localfilename.c_str(), "rb");
				if (fp == NULL) {
					cout << "[PUT] The file " << localfilename << " does not exist" << endl;
					continue;
				}
				fseek(fp, 0, SEEK_END);
				long int size = ftell(fp);
				fseek(fp, 0, SEEK_SET);
				fclose(fp);
				int number_of_lines = 0;
				string line;
				ifstream myfile(localfilename.c_str());
				while (getline(myfile, line)) ++number_of_lines;
				Messages outMsg(DNS, node->nodeInformation.ip + "::" + to_string(node->hashRingPosition) + "::" + sdfsfilename + "::" + localfilename + "::" + to_string(size) + "::" + to_string(number_of_lines) + "::" + "::" + "1");
				cout << "[PUT] Got localfilename: " << localfilename << " with sdfsfilename: " << sdfsfilename << endl;
				node->tcpServent->sendMessage(node->leaderIP, TCPPORT, outMsg.toString());
			} else {
				cout << "[BLACKOUT] Leader cannot accept the request" << endl;
			}
		} else if (cmd == "get" && joined){ // MP2 op2
			if(cmdLineInput.size() < 3){
				cout << "USAGE: get sdfsfilename filename" << endl;
				continue;
			}
			string sdfsfilename = cmdLineInput[1];
			string localfilename = cmdLineInput[2];
			if (node->localFilelist.find(sdfsfilename) != node->localFilelist.end()) {
				cout << "[GET] You have sdfsfilename " << sdfsfilename << " as " << node->localFilelist[sdfsfilename] << endl;
				continue;
			}
			if (node->fileList.find(sdfsfilename) == node->fileList.end()) {
				cout << "[GET] Get sdfsfilename " << sdfsfilename << " failed" << endl;
				continue;
			} else {
				if (node->fileList[sdfsfilename].size()==1 && node->isBlackout) {
					cout << "[GET] Get sdfsfilename " << sdfsfilename << " failed" << endl;
					continue;
				}
			}
			Messages outMsg(DNSGET, node->nodeInformation.ip + "::" + to_string(node->hashRingPosition) + "::" + sdfsfilename + "::" + localfilename);
			cout << "[GET] Got sdfsfilename: " << sdfsfilename << " with localfilename: " << localfilename << endl;
			node->tcpServent->sendMessage(node->leaderIP, TCPPORT, outMsg.toString());
		} else if (cmd == "delete" && joined){ // MP2 op3
			if(cmdLineInput.size() < 2){
				cout << "USAGE: delete sdfsfilename" << endl;
				continue;
			}
			if (!node->isBlackout) {
				string sdfsfilename = cmdLineInput[1];
				Messages outMsg(DELETE, node->nodeInformation.ip + "::" + sdfsfilename);
				cout << "[DELETE] Got sdfsfilename: " << sdfsfilename << endl;
				node->tcpServent->sendMessage(node->leaderIP, TCPPORT, outMsg.toString());
			} else {
				cout << "[BLACKOUT] Leader cannot accept the request" << endl;
			}
		} else if (cmd == "ls" && joined){ // MP2 op4
			if(cmdLineInput.size() < 2){
				cout << "USAGE: ls sdfsfilename" << endl;
				continue;
			}
			if (!node->isBlackout) {
				string sdfsfilename = cmdLineInput[1];
				node->listSDFSFileList(sdfsfilename);
			} else {
				cout << "[BLACKOUT] Leader cannot accept the request" << endl;
			}
		} else if (cmd == "store"){ // MP2 op5
			node->listLocalFiles();
		} else if (cmd == "lsall"){
			debugSDFSFileList(node);
		} else if (cmd == "maple" && joined){
			if(cmdLineInput.size() < 5){
				cout << "USAGE: maple maple_exe num_maples sdfs_intermediate_dir sdfs_src_dir" << endl;
				continue;
			}
			string testCommand = "./" + cmdLineInput[1] + " > /dev/null 2>&1";
			if (system(testCommand.c_str())) {
				cout << "[MAPLE] " << cmdLineInput[1] << " does not exist locally" << endl;
 			   	continue;
			}
			if (!node->isBlackout){
				string msg = cmdLineInput[1] + "," + cmdLineInput[2] + "," + cmdLineInput[3] + "," + cmdLineInput[4] + "\n";
				Messages outMsg(MAPLESTART, msg);
				cout << "[MAPLE] forwarding request to " << node->leaderIP << endl;
				node->tcpServent->sendMessage(node->leaderIP, TCPPORT, outMsg.toString());
			} else {
				cout << "[BLACKOUT] Leader cannot accept the request" << endl;
			}
		} else if (cmd == "juice" && joined){
			if(cmdLineInput.size() < 6){
				cout << "USAGE: juice juice_exe num_juices sdfs_intermediate_dir sdfs_out_file delete={0,1}" << endl;
				continue;
			}
			if (FILE *file = fopen(cmdLineInput[1].c_str(), "r")) {
				fclose(file);
			} else {
				cout << "[JUICE] " << cmdLineInput[1] << " does not exist locally" << endl;
				continue;
			}
			if (!node->isBlackout){
				//TODO
			} else {
				cout << "[BLACKOUT] Leader cannot accept the request" << endl;
			}
		} else if (cmd == "ip"){
			cout << getIP() << endl;
		} else if (cmd == "leader"){
			cout << "Leader IP " << node->leaderIP << endl;
		} else {
			cout << "[join] join to a group via fixed introducer" << endl;
			cout << "[leave] leave the group" << endl;
			cout << "[id] print id (IP/PORT)" << endl;
			cout << "[member] print all membership list" << endl;
			cout << "[switch] switch to other mode (All-to-All to Gossip, and vice versa)" << endl;
			cout << "[mode] show in 0/1 [All-to-All/Gossip] modes" << endl;
			cout << "[exit] terminate process" << endl;
			cout << " === New since MP2 === " << endl;
			cout << "[put] localfilename sdfsfilename" << endl;
			cout << "[get] sdfsfilename localfilename" << endl;
			cout << "[delete] sdfsfilename" << endl;
			cout << "[ls] list all machine (VM) addresses where this file is currently being stored" << endl;
			cout << "[lsall] list all sdfsfilenames with positions" << endl;
			cout << "[store] list all files currently being stored at this machine" << endl << endl;
		} // More command line interface if wanted
	}

	pthread_kill(threads[0], SIGUSR1);
	pthread_kill(threads[4], SIGUSR1);
	if(joined){
		pthread_kill(threads[1], SIGUSR1);
	}
	pthread_exit(NULL);
	return 1;
}
