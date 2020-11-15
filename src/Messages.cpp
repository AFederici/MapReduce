#include "../inc/Messages.h"

Messages::Messages(string payloadMessage)
{
	string delimiter = "::";
	size_t pos = 0;
	int line = 0;
	string token;
	while ((pos = payloadMessage.find(delimiter)) != string::npos) {
		token = payloadMessage.substr(0, pos);
//#ifdef LOG_VERBOSE
//		cout << line << "/" << token << endl;
//#endif
		switch (line) {
			case 0: {
				type = static_cast<MessageType>(stoi(token));
				break;
			}
			default:
				break;
		}
		payloadMessage.erase(0, pos + delimiter.length());
		line++;
	}
//#ifdef LOG_VERBOSE
//	cout << payloadMessage << endl;
//#endif
	payload = payloadMessage;
}

Messages::Messages(MessageType messageType, string payloadMessage)
{
	type = messageType;
	payload = payloadMessage;
}

string Messages::toString()
{
	string message = to_string(type) + "::" + payload;
	return message;
}
