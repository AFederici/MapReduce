#include "../inc/Node.h"

void computeAndPrintBW(Node * n, double diff)
{
#ifdef LOG_VERBOSE
	cout << "total " << n->udpServent->byteSent << " bytes sent" << endl;
	cout << "total " << n->udpServent->byteReceived << " bytes received" << endl;
	printf("elasped time is %.2f s\n", diff);
#endif
	if (diff > 0) {
		double bandwidth = n->udpServent->byteSent/diff;
		string message = "["+to_string(n->localTimestamp)+"] B/W usage: "+to_string(bandwidth)+" bytes/s";
#ifdef LOG_VERBOSE
		printf("%s\n", message.c_str());
#endif
		n->logWriter->printTheLog(BANDWIDTH, message);
	}
}

void debugMembershipList(Node * n)
{
	cout << "Membership list [" << n->membershipList.size() << "]:" << endl;
	if (n->isLeader) {
		cout << "[T]   IP/Port/JoinedTime:Heartbeat/LocalTimestamp/FailFlag" << endl;
	} else {
		cout << "[T] IP/Port/JoinedTime:Heartbeat/LocalTimestamp/FailFlag" << endl;
	}
	string message = "";

	for (auto& element: n->membershipList) {
		tuple<string,string,string> keyTuple = element.first;
		tuple<int, int, int> valueTuple = element.second;

		if (n->nodeInformation.ip.compare(get<0>(keyTuple))==0) { // Myself
			if (n->isLeader) {
				message += "[L/M] ";
			} else {
				message += "[M] ";
			}
		} else if (n->leaderIP.compare(get<0>(keyTuple))==0) {
			message += "[L] ";
		} else {
			if (n->isLeader) {
				message += "      ";
			} else {
				message += "    ";
			}
		}

		message += get<0>(keyTuple)+"/"+get<1>(keyTuple)+"/"+get<2>(keyTuple);
		message += ": "+to_string(get<0>(valueTuple))+"/"+to_string(get<1>(valueTuple))+"/"+to_string(get<2>(valueTuple))+"\n";
	}
	cout << message.c_str() << endl;
	n->logWriter->printTheLog(MEMBERS, message);
}

void debugSDFSFileList(Node * n) {
	cout << "sdfsfilename ---> positions,..." << endl;
	for (auto& element: n->fileList) {
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

void *runServerTEST(void *udpSocket)
{
	UdpSocket* udp;
	udp = (UdpSocket*) udpSocket;
	udp->bindServer("4950");
	pthread_exit(NULL);
}

void *runClientTEST(void *udpSocket)
{
	UdpSocket* udp;
	udp = (UdpSocket*) udpSocket;
	for (int i = 0; i < 3; i++) {
		sleep(2);
		udp->sendMessage("127.0.0.1", "4950", "test message");
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

void *runTcpServerTEST(void *tcpSocket)
{
	TcpSocket* tcp;
	tcp = (TcpSocket*) tcpSocket;
	tcp->bindServer(TCPPORT);
	pthread_exit(NULL);
}

void *runTcpClientTEST(void *tcpSocket)
{
	TcpSocket* tcp;
	tcp = (TcpSocket*) tcpSocket;
	sleep(1);
	// testing election
	Messages msg(ELECTION, "my_id");
	tcp->sendMessage("127.0.0.1", TCPPORT, msg.toString());

	// testing to send file
	for (int i=0; i<2; i++) {
		sleep(1);
		tcp->putFile("127.0.0.1", TCPPORT, "file_example_MP3_700KB.mp3", "file_example_MP3_700KB.mp3", "file_example_MP3_700KB.mp3");
	}
	pthread_exit(NULL);
}

/*
int main(int argc, char *argv[])
{
	int isTcp = 0;
	if (argc > 1 && (strcmp(argv[1], "tcp") == 0)) isTcp = 1;
	UdpSocket *udpSocket = new UdpSocket();
	TcpSocket *tcpSocket = new TcpSocket();
	pthread_t threads[2];
	int rc;
	void * socket = (isTcp) ? (void*)tcpSocket : (void*)udpSocket;
	void* (*serverFunc)(void*) = (isTcp) ? runTcpServerTEST : runServerTEST;
	void* (*clientFunc)(void*) = (isTcp) ? runTcpClientTEST : runClientTEST;
	if ((rc = pthread_create(&threads[0], NULL, serverFunc, socket)) != 0) {
		cout << "Error:unable to create thread," << rc << endl;
		exit(-1);
	}
	if ((rc = pthread_create(&threads[1], NULL, clientFunc, socket)) != 0) {
		cout << "Error:unable to create thread," << rc << endl;
		exit(-1);
	}

	if (!isTcp){
		if (strcmp(argv[1], "client") == 0) {
			udpSocket->sendMessage("127.0.0.1", "4950", "test message");
		} else {
			udpSocket->bindServer("4950");
		}
	}
	pthread_exit(NULL);
	return 0;
}
*/
