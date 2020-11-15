#include "../inc/TcpSocket.h"
#include "../inc/UdpSocket.h"
#include "../inc/Messages.h"
#include "../inc/FileObject.h"

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

TcpSocket::TcpSocket() 
{

}

void TcpSocket::bindServer(string port)
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes = 1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	char buf[DEFAULT_TCP_BLKSIZE];
	int numbytes;
	string delimiter = "::";

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, port.c_str(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return;
	}

	// loop through all the results and bind to the first we can
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

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
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

	//printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		//printf("server: got connection from %s\n", s);

		bzero(buf, sizeof(buf));
		MessageType type = JOIN; // for default case here
		string payload = "";
		if ((numbytes = recv(new_fd, buf, DEFAULT_TCP_BLKSIZE, 0)) > 0) {
			//buf[numbytes] = '\0';
			//printf("Got %s\n", buf);

			string payloadMessage(buf);
			Messages msg(payloadMessage);
			//printf("message type: %d\n", msg.type);
			type = msg.type;
			payload = msg.payload;
		}

		switch (type) {
			case ELECTION:
			case ELECTIONACK: {
				//cout << "election id is " << payload << endl;
				string payloadMessage(buf);
				//cout << "payloadMessage " << payloadMessage << endl;
				qMessages.push(payloadMessage);
				//cout << "qMessages size 1 " << qMessages.size() << endl;
				break;
			}
			case PUT: {
				FILE *fp;
				int filesize = 0;
				int byteReceived = 0;
				string sdfsfilename = "";
				string incomingChecksum = "";
				string remoteLocalname = "";
				string overwriteFilename = "";
				// format: size,checksum,sdfsfilename
				vector<string> fields = splitString(payload, ",");
				if (fields.size() >= 5) {
					filesize = stoi(fields[0]);
					incomingChecksum = fields[1];
					sdfsfilename = fields[2];
					remoteLocalname = fields[3];
					overwriteFilename = fields[4];
				}
				cout << "file is " << sdfsfilename << " with size " << filesize << " and checksum " << incomingChecksum << endl;

				time_t fileTimestamp;
				time(&fileTimestamp);
				string localfilename = sdfsfilename+"_"+to_string(fileTimestamp);
				if (overwriteFilename.compare("") != 0) {
					localfilename = overwriteFilename;
					cout << "it's GET with filename " << overwriteFilename << endl;
				}
				cout << "backup filename " << localfilename << endl;
				fp = fopen(localfilename.c_str(), "wb");
				if (fp == NULL) {
					cout << "file error" << endl;
					close(new_fd);
					exit(0);
				}

				bzero(buf, sizeof(buf));
				while ((numbytes=recv(new_fd, buf, DEFAULT_TCP_BLKSIZE, 0)) > 0) {
					//printf("Got %d\n", numbytes);
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
				if(incomingChecksum.compare(f.checksum) != 0){
					cout << "[ERROR] FILE CORRUPTED" << endl;
					// TODO: Handel file corruption here
				} else {
					string returnIP(s);
					Messages putack(PUTACK, returnIP + "::" + sdfsfilename + "::" + localfilename+"::"+remoteLocalname);
					regMessages.push(putack.toString());
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
				string payloadMessage(buf);
				cout << "Type: " << type << " payloadMessage: " << payloadMessage << endl;
				regMessages.push(payloadMessage);
				break;
			}
			default:
				break;
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

void TcpSocket::sendFile(string ip, string port, 
	string localfilename, string sdfsfilename, string remoteLocalfilename)
{
	int sockfd, numbytes;  
	char buf[DEFAULT_TCP_BLKSIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	FILE *fp;
	int size = 0;

	bzero(buf, sizeof(buf));

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(ip.c_str(), port.c_str(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return;
	}

	// loop through all the results and connect to the first we can
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
		return;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	// read file
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
	Messages msg(PUT, getFileMetadata(size, f.checksum, sdfsfilename, localfilename, remoteLocalfilename));
	string payload = msg.toString();
	
	if (send(sockfd, payload.c_str(), strlen(payload.c_str()), 0) == -1) {
		perror("send");
	}
	sleep(1);

	while (!feof(fp) && size > 0) {
		if (size < DEFAULT_TCP_BLKSIZE) {
			bzero(buf, sizeof(buf));
			numbytes = fread(buf, sizeof(char), size, fp);
			//printf("11 numbytes %d, size %d\n", numbytes, size);
			if (send(sockfd, buf, numbytes, 0) == -1) {
				perror("send");
			}
		} else {
			bzero(buf, sizeof(buf));
			numbytes = fread(buf, sizeof(char), DEFAULT_TCP_BLKSIZE, fp);
			//printf("22 numbytes %d, size %d\n", numbytes, size);
			size -= numbytes;
			//printf("33 numbytes %d, size %d\n", numbytes, size);
			if (send(sockfd, buf, numbytes, 0) == -1) {
				perror("send");
			}
		}
		
	}
	fclose(fp);
	close(sockfd);
}

void TcpSocket::sendMessage(string ip, string port, string message)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(ip.c_str(), port.c_str(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return;
	}

	// loop through all the results and connect to the first we can
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
		return;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	//printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	if (send(sockfd, message.c_str(), strlen(message.c_str()), 0) == -1) {
		perror("send");
	}

	close(sockfd);
}

// copy from Node.cpp
vector<string> TcpSocket::splitString(string s, string delimiter){
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

// void *runTcpServer(void *tcpSocket)
// {
// 	TcpSocket* tcp;
// 	tcp = (TcpSocket*) tcpSocket;
// 	tcp->bindServer(TCPPORT);
// 	pthread_exit(NULL);
// }

// void *runTcpClient(void *tcpSocket) 
// {
// 	TcpSocket* tcp;
// 	tcp = (TcpSocket*) tcpSocket;
// 	sleep(1);
// 	// testing election
// 	Messages msg(ELECTION, "my_id");
// 	tcp->sendMessage("127.0.0.1", TCPPORT, msg.toString());

// 	// testing to send file
// 	for (int i=0; i<2; i++) {
// 		sleep(1);
// 		tcp->sendFile("127.0.0.1", TCPPORT, "file_example_MP3_700KB.mp3");
// 	}
// 	pthread_exit(NULL);
// }

/*int main(int argc, char *argv[]) 
{
	TcpSocket *tcpSocket = new TcpSocket();

	pthread_t threads[2];

	int rc;

	if ((rc = pthread_create(&threads[0], NULL, runTcpServer, (void *)tcpSocket)) != 0) {
		cout << "Error:unable to create thread," << rc << endl;
		exit(-1);
	}

	if ((rc = pthread_create(&threads[1], NULL, runTcpClient, (void *)tcpSocket)) != 0) {
		cout << "Error:unable to create thread," << rc << endl;
		exit(-1);
	}
	
	pthread_exit(NULL);

	return 0;
}*/