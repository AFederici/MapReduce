#include "../inc/TcpSocket.h"

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

TcpSocket::TcpSocket(){}

void TcpSocket::bindServer(string port)
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes = 1, rv = 0, numbytes = 0;
	char buf[DEFAULT_TCP_BLKSIZE];
	string delimiter = "::";
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, port.c_str(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return;
	}

	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}
		if (::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}
		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		char remoteIP[INET6_ADDRSTRLEN];
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) { perror("accept"); continue; }
		inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),remoteIP, sizeof(remoteIP));
		bzero(buf, sizeof(buf));
		if ((numbytes = recv(new_fd, buf, DEFAULT_TCP_BLKSIZE, 0)) > 0) {
			string payloadMessage(buf);
			string returnIP(remoteIP);
			if (messageHandler(new_fd, payloadMessage, returnIP)) continue;
		}
		close(new_fd);
	}
}

string TcpSocket::getFileMetadata(int size, string checksum,
	string sdfsfilename, string localfilename, string remoteLocalfilename, string overwrite)
{
	// format: size,checksum,sdfsfilename
	string msg = to_string(size) + "," + checksum + "," + sdfsfilename+","+localfilename+","+remoteLocalfilename+","+overwrite;
	return msg;
}

void TcpSocket::sendFile(string ip, string port,
	string localfilename, string sdfsfilename, string remoteLocalfilename, string overwrite)
{
	int numbytes, sockfd;
	char buf[DEFAULT_TCP_BLKSIZE];
	FILE *fp;
	int size = 0, sendSize = 0;
	bzero(buf, sizeof(buf));
	if ((sockfd = createConnection(ip, port)) == -1) return;
	fp = fopen(localfilename.c_str(), "rb");
	if (fp == NULL) {
		printf("Could not open file to send.");
		return;
	}
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	// send bytes and filename first
	FileObject f(localfilename);
	Messages msg(PUT, getFileMetadata(size, f.checksum, sdfsfilename, localfilename, remoteLocalfilename, overwrite));
	string payload = msg.toString();
	if (send(sockfd, payload.c_str(), strlen(payload.c_str()), 0) == -1) {
		perror("send");
	}
	sleep(1);
	while (!feof(fp) && size > 0) {
		sendSize = (size < DEFAULT_TCP_BLKSIZE) ? size : DEFAULT_TCP_BLKSIZE;
		bzero(buf, sizeof(buf));
		numbytes = fread(buf, sizeof(char), sendSize, fp);
		size -= numbytes;
		if (send(sockfd, buf, numbytes, 0) == -1) {
			perror("send");
		}
	}
	fclose(fp);
	close(sockfd);
}

void TcpSocket::sendLines(string ip, string port, string execfile, string readfile, string prefix, int start, int end)
{
	int sockfd = 0, lineCounter = -1;
	if ((sockfd = createConnection(ip, port)) == -1) return;
	//exec, read, start, tmp, prefix
	string toSend = execfile + "," + readfile + "," + to_string(start) + "," + readfile + to_string(start) + "temp" + "," + prefix;
	Messages msg(PUT, toSend);
	string payload = msg.toString();
	if (send(sockfd, payload.c_str(), strlen(payload.c_str()), 0) == -1) {
		perror("send");
	}
	sleep(1);
	ifstream file(readfile.c_str());
    string str;
    while (getline(file, str))
    {
		lineCounter++;
        if (lineCounter < start) continue;
		if (lineCounter >= end) break;
		if (send(sockfd, str.c_str(), strlen(str.c_str()), 0) == -1) {
			perror("send");
		}
    }
	close(sockfd);
}

void TcpSocket::sendMessage(string ip, string port, string message)
{
	int sockfd;
	if ((sockfd = createConnection(ip, port)) == -1) return;
	if (send(sockfd, message.c_str(), strlen(message.c_str()), 0) == -1) {
		perror("send");
	}
	close(sockfd);
}

int TcpSocket::createConnection(string ip, string port){
	int sockfd, rv;
	struct addrinfo hints, *servinfo, *p;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if ((rv = getaddrinfo(ip.c_str(), port.c_str(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}
		break;
	}
	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return -1;
	}
	freeaddrinfo(servinfo);
	return sockfd;
}

int TcpSocket::messageHandler(int sockfd, string payloadMessage, string returnIP){
	char buf[DEFAULT_TCP_BLKSIZE];
	int numbytes = 0;
	Messages msg(payloadMessage);
	switch (msg.type) {
		case ELECTION:
		case ELECTIONACK: {
			qMessages.push(payloadMessage);
			break;
		}
		case PUT: {
			FILE *fp;
			int filesize = 0, byteReceived = 0;
			string mode = "wb";
			string sdfsfilename = "", incomingChecksum = "", remoteLocalname = "", overwriteFilename = "", prefix = "", overwrite = "", localfilename = "";
			// format: size,checksum,sdfsfilename
			vector<string> fields = splitString(msg.payload, ",");
			int start = -1;
			if (fields.size() >= 6) {
				filesize = stoi(fields[0]);
				incomingChecksum = fields[1];
				sdfsfilename = fields[2];
				remoteLocalname = fields[3];
				overwriteFilename = fields[4];
				overwrite = fields[5];
				if ((stoi(overwrite)) == 0) mode = "ab";
				cout << "file is " << sdfsfilename << " with size " << filesize << " and checksum " << incomingChecksum << endl;
				time_t fileTimestamp;
				time(&fileTimestamp);
				localfilename = sdfsfilename+"_"+to_string(fileTimestamp);
				if (overwriteFilename.compare("") != 0) {
					localfilename = overwriteFilename;
					cout << "it's GET with filename " << overwriteFilename << endl;
				}
				cout << "backup filename " << localfilename << endl;
			} else {
				//exec, read, start, tmp, prefix
				localfilename = fields[3]; //tempfile to read from
				sdfsfilename = fields[0]; //exec file name
				start = stoi(fields[2]); //start line (used just for signalling what work finished to master)
				remoteLocalname = fields[1]; //actual file (used for signalling)
				prefix = fields[4];
			}
			fp = fopen(localfilename.c_str(), mode.c_str());
			if (fp == NULL) {
				cout << "file error" << endl;
				close(sockfd);
				return 1;
			}

			bzero(buf, sizeof(buf));
			while ((numbytes=recv(sockfd, buf, DEFAULT_TCP_BLKSIZE, 0)) > 0) {
				fwrite(buf, sizeof(char), numbytes, fp);
				byteReceived += numbytes;
				if (byteReceived >= filesize) {
					break;
				}
				bzero(buf, sizeof(buf));
			}
			cout << "we have all the file, finishing this connections" << endl;
			fclose(fp);

			FileObject f(localfilename);
			if(incomingChecksum.compare(f.checksum) != 0 && incomingChecksum.compare("") != 0){
				cout << "[ERROR] FILE CORRUPTED" << endl;
				// TODO: Handel file corruption here
			} else {
				if (start != -1){
					//IP, exec, start, temp, actual file, prefix
					Messages putack(CHUNKACK, returnIP + "::" + sdfsfilename + "::" + to_string(start) + "::" + localfilename + "::" + remoteLocalname + "::" + prefix);
					regMessages.push(putack.toString());
				} else {
					Messages putack(PUTACK, returnIP + "::" + sdfsfilename + "::" + localfilename+"::"+remoteLocalname);
					regMessages.push(putack.toString());
				}
			}
			break;
		}
		case DNSANS:
		case ACK:
		case PUTACK:
		case LEADERACK:
		case REREPLICATE:
		case REREPLICATEGET:
		case DNSGET:
		case DELETE:
		case GETNULL:
		case DNS:{
			cout << "Type: " << msg.type << " payloadMessage: " << payloadMessage << endl;
			regMessages.push(payloadMessage); //handle from queue
			break;
		}
		default:
			break;
	}
	return 0;
}
