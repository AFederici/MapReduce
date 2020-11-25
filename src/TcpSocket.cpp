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
	string sdfsfilename, string localfilename, string remoteLocalfilename)
{
	// format: size,checksum,sdfsfilename
	string msg = to_string(size) + "," + checksum + "," + sdfsfilename+","+localfilename+","+remoteLocalfilename;
	return msg;
}

string TcpSocket::getDirMetadata()
{
	struct dirent *entry = nullptr;
    DIR *dp = nullptr;
	FILE * fp;
    string match = "tmp-";
    int matchLen = match.size();
	vector<string> split;
	int size = 0;
	string msg;
    if ((dp = opendir(".")) == nullptr) { cout << "tmp directory error " << endl;}
    while ((entry = readdir(dp))){
        if (strncmp(entry->d_name, match.c_str(), matchLen) == 0){
			split.clear();
            split = splitString(entry->d_name, "-");
			if (split.size() > 2) continue;
			fp = fopen(entry->d_name, "rb");
			if (fp == NULL) {
				printf("Could not open file to send.");
				continue;
			}
			fseek(fp, 0, SEEK_END);
			size = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			if (msg.size()) msg += ",";
			msg += entry->d_name;
			msg += ",";
			msg += to_string(size);
			fclose(fp);
        }
    }
	closedir(dp);
	return msg;
}

void TcpSocket::putDirectory(string ip, string port) {
	FILE * fp;
	int sockfd = -1, index = 0;
	string toSend = getDirMetadata();
	if (!toSend.size()) return;
	vector<string> toProcess = splitString(toSend, ",");
	int dirSize = toProcess.size();
	cout << "[PUTDIR] " << toSend << endl;
	Messages msg(MERGE, toSend);
	if ((sockfd = createConnection(ip, port)) == -1) return;
	if (send(sockfd, msg.toString().c_str(), strlen(msg.toString().c_str()), 0) == -1) {
		perror("send");
	}
	sleep(1);
	while (index < dirSize - 1){
		fp = fopen(toProcess[index].c_str(), "rb");
		if (fp == NULL) {
			printf("Could not open file to send %s.", toProcess[index].c_str());
			continue;
		}
		sendFile(sockfd, fp, stoi(toProcess[index+1]));
		fclose(fp);
		index += 2;
	}
	close(sockfd);
}


void TcpSocket::putFile(string ip, string port, string localfilename, string sdfsfilename, string remoteLocalfilename){
	int sockfd = -1;
	FILE *fp = fopen(localfilename.c_str(), "rb");
	if (fp == NULL) {
		printf("Could not open file to send.");
		return;
	}
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	fclose(fp);
	cout << "[DOSEND] to nodeIP " << ip << ", localfilename " << localfilename << ", sdfsfilename " << sdfsfilename << endl;
	FileObject f(localfilename);
	Messages msg(PUT, getFileMetadata(size, f.checksum, sdfsfilename, localfilename, remoteLocalfilename));
	if ((sockfd = createConnection(ip, port)) == -1) return;
	if (send(sockfd, msg.toString().c_str(), strlen(msg.toString().c_str()), 0) == -1) {
		perror("send");
	}
	fp = fopen(localfilename.c_str(), "rb");
	sleep(1);
	sendFile(sockfd, fp, size);
	fclose(fp);
	close(sockfd);
}

void TcpSocket::sendFile(int sockfd, FILE * fp, int size) {
	int numbytes, sendSize;
	int startSize = size;
	char buf[DEFAULT_TCP_BLKSIZE];
	bzero(buf, sizeof(buf));
	while (!feof(fp) && size > 0) {
		sendSize = (size < DEFAULT_TCP_BLKSIZE) ? size : DEFAULT_TCP_BLKSIZE;
		bzero(buf, sizeof(buf));
		numbytes = fread(buf, sizeof(char), sendSize, fp);
		size -= numbytes;
		if (send(sockfd, buf, numbytes, 0) == -1) {
			perror("send");
		}
	}
	int bytesSent = startSize - size;
	cout << "[SENDFILE] sent: " << to_string(bytesSent) << endl;
}

//exec, file, start, end
void TcpSocket::sendLines(string ip, string port, string execFile, string sdfsFile, string localFile, int start, int end)
{
	int sockfd = -1, lineCounter = -1, numbytes = 0;
	ifstream file(localFile);
    string str;
    while (getline(file, str))
    {
		lineCounter++;
        if (lineCounter < start) continue;
		if (lineCounter >= end) break;
		numbytes += (str.size() + 1);
    }
	file.clear();  // clear fail and eof bits
	file.seekg(0); // back to the start!
	lineCounter = -1;
	vector<string> unDirectory = splitString(sdfsFile, "-"); //get rid of timestamp
	string tempName = "tmp-"+to_string(start)+"-"+sdfsFile.substr(unDirectory[0].size()+1);
	string toSend = to_string(numbytes) + "," + execFile + "," + localFile + "," + to_string(start) + "," + tempName + "," + sdfsFile;
	Messages msg(PUT, toSend);
	cout << "[CHUNK] message (ignore ::) " << msg.toString() << endl;
	string payload = msg.toString();
	if ((sockfd = createConnection(ip, port)) == -1) return;
	if (send(sockfd, payload.c_str(), strlen(payload.c_str()), 0) == -1) {
		perror("send");
	}
	sleep(1);
    while (getline(file, str))
    {
		lineCounter++;
        if (lineCounter < start) continue;
		else if (lineCounter == start) cout << "[CHUNK] starting to send at line " << to_string(lineCounter) << endl;
		if (lineCounter >= end) { cout << "[CHUNK] Counter at " << to_string(lineCounter) << " end: " << to_string(end) << endl; break; }
		str += '\n';
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
	int numbytes = 0, filesize = 0, byteReceived = 0;
	FILE *fp;
	Messages msg(payloadMessage);
	switch (msg.type) {
		case ELECTION:
		case ELECTIONACK: {
			qMessages.push(payloadMessage);
			break;
		}
		case MERGE: {
			vector<string> filesAndSizes = splitString(msg.payload, ",");
			int dirSize = filesAndSizes.size();
			int index = 0;
			int fail = 0;
			vector<string> format;
			while (index < dirSize - 1){
				format.clear();
				format = splitString(filesAndSizes[index], "-"); //cut the tmp off
				string filename = "tmp-" + returnIP + "-" + format[1];
				filesize = stoi(filesAndSizes[index+1]);
				numbytes = 0;
				byteReceived = 0;
				fp = fopen(filename.c_str(), "wb");
				bzero(buf, sizeof(buf));
				while ((numbytes=recv(sockfd, buf, DEFAULT_TCP_BLKSIZE, 0)) > 0) {
					fwrite(buf, sizeof(char), numbytes, fp);
					byteReceived += numbytes;
					if (byteReceived >= filesize) {
						break;
					}
					bzero(buf, sizeof(buf));
				}
				if (byteReceived < filesize) fail = 1;
				//cout << "we have " << to_string(byteReceived) << " bytes from this connection" << endl;
				fclose(fp);
				index += 2;
			}
			if (fail) { Messages ack(MERGEFAIL, returnIP + "::"); regMessages.push(ack.toString()); }
			else { Messages ack(MERGECOMPLETE, returnIP + "::"); regMessages.push(ack.toString()); }
			break;
		}
		case PUT: {
			string sdfsfilename = "", incomingChecksum = "", remoteLocalname = "", overwritefilename = "", localfilename = "", execfilename = "";
			// format: size,checksum,sdfsfilename
			vector<string> fields = splitString(msg.payload, ",");
			int start = -1;
			if (fields.size() == 5) {
				filesize = stoi(fields[0]);
				incomingChecksum = fields[1];
				sdfsfilename = fields[2];
				remoteLocalname = fields[3];
				overwritefilename = fields[4];
				cout << "[PUT] file is " << sdfsfilename << " with size " << filesize << " and checksum " << incomingChecksum << endl;
				time_t fileTimestamp;
				time(&fileTimestamp);
				localfilename = sdfsfilename+"_"+to_string(fileTimestamp);
				if (overwritefilename.compare("") != 0) {
					localfilename = overwritefilename;
					//cout << "it's GET with filename " << overwriteFilename << endl;
				}
				//cout << "backup filename " << localfilename << endl;
			}
			if (fields.size() == 6){
				//size, exec, read, start, tmp, prefix
				filesize = stoi(fields[0]);
				localfilename = fields[4]; //tempfile to read from
				execfilename = fields[1]; //exec file name
				start = stoi(fields[3]); //start line (used just for signalling what work finished to master)
				remoteLocalname = fields[2]; //actual file (used for signalling)
				sdfsfilename = fields[5]; //sdfs file
				cout << "[PUT] bytes: " << filesize << " exec: " << execfilename << ", actual: " << remoteLocalname << ", start: " << to_string(start) << ", temp: " << localfilename << endl;
			}
			fp = fopen(localfilename.c_str(), "wb");
			bzero(buf, sizeof(buf));
			while ((numbytes=recv(sockfd, buf, DEFAULT_TCP_BLKSIZE, 0)) > 0) {
				fwrite(buf, sizeof(char), numbytes, fp);
				byteReceived += numbytes;
				if (byteReceived >= filesize) {
					break;
				}
				bzero(buf, sizeof(buf));
			}
			cout << "we have " << to_string(byteReceived) << " bytes from this connection" << endl;
			fclose(fp);

			//FileObject f(localfilename);
			//if(incomingChecksum.compare(f.checksum) != 0 && incomingChecksum.compare("") != 0){
			//	cout << "[ERROR] FILE CORRUPTED" << endl;
				// how to deal with?
			//} else {
				if (start != -1){
					//IP, exec, start, temp, sdfs file
					Messages putack(CHUNKACK, returnIP + "::" + execfilename + "::" + to_string(start) + "::" + localfilename + "::" + sdfsfilename);
					regMessages.push(putack.toString());
				} else {
					Messages putack(PUTACK, returnIP + "::" + sdfsfilename + "::" + localfilename+"::"+remoteLocalname);
					regMessages.push(putack.toString());
				}
			//}
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
		case MAPLESTART:
		case MAPLEACK:
		case CHUNK:
		case CHUNKACK:
		case STARTMERGE:
		case MERGECOMPLETE:
		case MERGEFAIL:
		case DNS:{
			//cout << "["<< messageTypes[msg.type] << "] payloadMessage: " << payloadMessage << endl;
			regMessages.push(payloadMessage); //handle from queue
			break;
		}
		default:
			break;
	}
	return 0;
}
