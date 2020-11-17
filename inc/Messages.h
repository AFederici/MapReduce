#ifndef MESSAGES_H
#define MESSAGES_H

#include <iostream>
#include <string>
#include "MessageTypes.h"

using namespace std;

class Messages {
public:
	MessageType type;
	string payload;
	Messages(string payloadMessage);
	Messages(MessageType messageType, string payloadMessage);
	string toString();
};

#endif //MESSAGES_H
