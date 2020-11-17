#include "../inc/Logger.h"

Logger::Logger() { filename = LOGFILE; }

Logger::Logger(string fileName) { filename = fileName; }

int Logger::writeToFile(string messages)
{
	FILE *fp;
	fp = fopen(filename.c_str(), "a");
	fprintf(fp, "%s", messages.c_str());
	fclose(fp);
	return 0;
}

int Logger::printTheLog(LogType type, string s)
{
	switch (type) {
		case JOINGROUP: {
			string messages = "[JOIN]"+s+"\n";
			writeToFile(messages);
			break;
		}
		case UPDATE: {
			string messages = "[UPDATE]"+s+"\n";
			writeToFile(messages);
			break;
		}
		case FAIL: {
			string messages = "[FAIL]"+s+"\n";
			writeToFile(messages);
			break;
		}
		case REMOVE: {
			string messages = "[REMOVE]"+s+"\n";
			writeToFile(messages);
			break;
		}
		case LEAVE: {
			string messages = "[LEAVE]"+s+"\n";
			writeToFile(messages);
			break;
		}
		case GOSSIPTO: {
			string messages = "[SENT]"+s+"\n";
			writeToFile(messages);
			break;
		}
		case GOSSIPFROM: {
			string messages = "[RECEIVED]"+s+"\n";
			writeToFile(messages);
			break;
		}
		case BANDWIDTH: {
			string messages = "[BANDWIDTH]"+s+"\n";
			writeToFile(messages);
			break;
		}
		case MEMBERS: {
			string messages = "[MembershipList] "+s+"\n";
			writeToFile(messages);
			break;
		}
		default:
			break;
	}
	return 0;
}
