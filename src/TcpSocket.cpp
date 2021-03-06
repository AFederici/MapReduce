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

void TcpSocket::mergeFiles(string ip, string port, string handler, string filedest, string toSend) {
	FILE * fp;
	int sockfd = -1, index = 0;
	if (!toSend.size()) return;
	vector<string> toProcess = splitString(toSend, ",");
	int dirSize = toProcess.size();
	string payload = handler + "," + filedest + "," + toSend;
	payload = to_string(payload.size()) + "," + payload;
	cout << "[PUTDIR] payload: " << payload << " to " << ip << endl;
	Messages msg(MERGE, payload);
	if ((sockfd = createConnection(ip, port)) == -1) return;
	if (send(sockfd, msg.toString().c_str(), strlen(msg.toString().c_str()), 0) == -1) {
		perror("send");
	}
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
}

//exec, file, start, end
void TcpSocket::sendLines(string ip, string port, string execFile, string sdfsFile, string localFile, int start, int end)
{
	int sockfd = -1, lineCounter = -1, numbytes = 0, readLines = 0;
	ifstream file(localFile);
    string str;
    while (getline(file, str) && (lineCounter < end - 1))
    {
		lineCounter++;
        if (lineCounter < start) continue;
		numbytes += (str.size() + 1);
		readLines++;
    }
	cout << "[SENDLINES] " << to_string(readLines) << endl;
	file.clear();  // clear fail and eof bits
	file.seekg(0); // back to the start!
	lineCounter = -1;
	vector<string> unDirectory = splitString(sdfsFile, "-"); //get rid of timestamp
	string tempName = "tmp-"+to_string(start)+"-"+sdfsFile.substr(unDirectory[0].size()+1);
	string toSend = to_string(numbytes) + "," + execFile + "," + localFile + "," + to_string(start) + "," + tempName + "," + sdfsFile;
	Messages msg(PUT, toSend);
	//cout << "[CHUNK] message (ignore ::) " << msg.toString() << endl;
	string payload = msg.toString();
	if ((sockfd = createConnection(ip, port)) == -1) return;
	if (send(sockfd, payload.c_str(), strlen(payload.c_str()), 0) == -1) {
		perror("send");
	}
	sleep(1);
    while (getline(file, str) && (lineCounter < end - 1))
    {
		lineCounter++;
        if (lineCounter < start) continue;
		if (lineCounter == start) //cout << "[CHUNK] starting to send at line " << to_string(lineCounter) << endl;
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
			cout << "[MERGE] merging ..... " << msg.payload << endl;
			vector<string> metainfo = splitString(msg.payload, ",");
			//correction if merge information made it into the header
			string payload = msg.payload.substr(metainfo[0].size() + 1, stoi(metainfo[0])), extra = "";
			try { extra = msg.payload.substr(metainfo[0].size() + 1 + stoi(metainfo[0])); }
			catch (const out_of_range&) { extra = ""; }
			vector<string> filesAndSizes = splitString(payload, ",");
			int returnType = stoi(filesAndSizes[0]);
			string returnTypeString = ((returnType == MAPLEACK)) ? "MAPLE" : "JUICE";
			char c;
			string filedest = filesAndSizes[1], processed = "", filename = "";
			cout << "[MERGE] type:" << returnTypeString << ", correction to extra -> " << extra << endl;
			int dirSize = filesAndSizes.size(), index = 2, fail = 0, filesize = 0;
			int bytesLeft = 0, offset = extra.size(), buffersize = DEFAULT_TCP_BLKSIZE;
			vector<string> format;
			while (index < dirSize - 1){
				format.clear();
				string scopy(filesAndSizes[index]);
				format = splitString(scopy, "-"); //cut the tmp off
				filename = (filedest.size()) ? filedest : "tmp-" + returnIP + "-" + format[1];
				cout << "[MERGE] (2-indexed):" << to_string(index) << " , dest:" << filename << " , size:" << filesAndSizes[index+1] << endl;
				numbytes = 0;
				filesize = stoi(filesAndSizes[index+1]);
				bytesLeft = filesize;
				buffersize = DEFAULT_TCP_BLKSIZE;
				buffersize = (bytesLeft < buffersize) ? bytesLeft : DEFAULT_TCP_BLKSIZE;
				fp = fopen(filename.c_str(), "ab");
				bzero(buf, sizeof(buf));
				if (extra.size()) {
					offset = extra.size();
					offset = (offset <= buffersize) ? offset : buffersize;
					memcpy(buf, extra.c_str(), offset);
				}
				cout << "		bytesleft:" << to_string(bytesLeft) << ", offset: " << to_string(offset) << endl;
				while ((((numbytes=recv(sockfd, buf + offset, buffersize - offset, 0)) >= 0) || (offset > 0)) && (bytesLeft > 0)) {
					bytesLeft -= numbytes;
					bytesLeft -= offset;
					if (bytesLeft >= 0) fwrite(buf, sizeof(char), numbytes + offset, fp);
					buffersize = (bytesLeft < buffersize) ? bytesLeft : DEFAULT_TCP_BLKSIZE;
					bzero(buf, sizeof(buf));
					if (offset > 0){
						try { extra = extra.substr(offset); }
						catch ( const out_of_range&) { extra = ""; }
						offset = extra.size();
						offset = (offset <= buffersize) ? offset : buffersize;
						memcpy(buf, extra.c_str(), offset);
					}
					cout << "		bytesleft:" << to_string(bytesLeft) << ", offset: " << to_string(offset) << ", numbytes: " << to_string(numbytes) << endl;
				}
				fclose(fp);
				////bad if corrupt
				if (bytesLeft) {
					cout <<"[MERGE] file corruption! bytesLeft: " << to_string(bytesLeft);
					fail = 1;
					while ((numbytes < 0) && (bytesLeft > 0) && (offset > 0)){
						try { extra = extra.substr(bytesLeft); }
						catch ( const out_of_range&) { extra = ""; }
						offset = extra.size();
						buffersize = (bytesLeft < buffersize) ? bytesLeft : DEFAULT_TCP_BLKSIZE;
						offset = (offset <= buffersize) ? offset : buffersize;
						bytesLeft -= offset;
						cout << ". error fix, move offet ahead: " << to_string(offset);
					}
					if (returnType == MAPLEACK) remove(filename.c_str());
					else {
						int removal = (filesize - bytesLeft);
						fp = fopen(filename.c_str(), "rb");
						fseek(fp, 0, SEEK_END);
						int size = ftell(fp) - removal;
						fseek(fp, 0, SEEK_SET);
						FILE * copyFile = fopen("tmp-rewrite-corrupt-file", "ab");
						cout <<" | removing " << to_string(removal) << " bytes";
						c = fgetc(fp);
					    while (c != EOF && (size > 0))
					    {
					        fputc(c, copyFile);
					        c = fgetc(fp);
							size--;
					    }
						fclose(fp);
						remove(filename.c_str());
						fclose(copyFile);
						rename("tmp-rewrite-corrupt-file", filename.c_str());
					}
					cout << endl;
				}
				else {
					if (processed.size()) processed += ",";
					//return list of processed keys. Manipulate this in JUICE ack to account for directories
					processed += format[1];
					cout << "[MERGE] processed: " << processed << endl;
				}
				index += 2;
			}
			if (fail && (returnType == MAPLEACK)) { Messages ack(MERGEFAIL, returnIP + "::"); regMessages.push(ack.toString()); }
			else if (returnType == MAPLEACK){ Messages ack(MERGECOMPLETE, returnIP + "::"); regMessages.push(ack.toString()); }
			else if (returnType == JUICE){ Messages ack(JUICEACK, returnIP + "::" + processed); regMessages.push(ack.toString()); }
			else { cout << "[MERGE bad return type " << to_string(returnType) << endl;}
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
				// in the future deal with file corruption
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
		case JUICESTART:
		case PHASESTART:
		case MAPLEACK:
		case CHUNK:
		case CHUNKACK:
		case JUICE:
		case JUICEACK:
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
