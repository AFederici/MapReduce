#include "../inc/UdpSocket.h"

UdpSocket::UdpSocket(){
	byteSent = 0;
	byteReceived = 0;
}

void UdpSocket::bindServer(string port)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes = 0;
	struct sockaddr_storage their_addr;
	char buf[MAXBUFLEN];
	socklen_t addr_len;
	memset(&hints, 0, sizeof hints);

	hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
	hints.ai_socktype = SOCK_DGRAM; // UDP
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, port.c_str(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("bindServer: socket");
			continue;
		}

		if (::bind(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
			close(sockfd);
			perror("bindServer: bind");
			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "bindServer: failed to bind socket\n");
		return;
	}

	freeaddrinfo(servinfo);

	//cout << "bindServer: waiting to recvfrom... " << endl;

	addr_len = sizeof(their_addr);
	bzero(buf, sizeof(buf));
	while ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
		(struct sockaddr *)&their_addr, &addr_len)) > 0) {
		this->byteReceived += numbytes;
		//cout << "bindServer: from " << inet_ntop(their_addr.ss_family,
		//	get_in_addr((struct sockaddr *)&their_addr),
		//	s, sizeof(s));
		//cout << "bindServer: packet is " << numbytes << " bytes long" << endl;
		buf[numbytes] = '\0';
		//cout << ": " << buf << endl;
		qMessages.push(buf);
		bzero(buf, sizeof(buf));
	}

	close(sockfd);
}

void UdpSocket::sendMessage(string ip, string port, string message)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes = 0;
	int lucky_number;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(ip.c_str(), port.c_str(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return;
	}

	// loop through all the results and make a socket
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("sendMessage: socket");
			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "sendMessage: failed to bind socket\n");
		return;
	}

	// Simulate package loss
	srand(time(NULL));
	lucky_number = rand() % 100 + 1;

	this->byteSent += strlen(message.c_str());
	if(lucky_number > LOSS_RATE){
		numbytes = sendto(sockfd, message.c_str(), strlen(message.c_str()), 0, p->ai_addr, p->ai_addrlen);
	}

	if (numbytes == -1) {
		perror("sendMessage: sendto");
		exit(1);
	}

	freeaddrinfo(servinfo);
	//cout << "sendMessage: sent " << numbytes << " bytes to " << ip << endl;
	close(sockfd);
}
