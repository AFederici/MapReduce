#ifndef LOGGER_H
#define LOGGER_H

#include <iostream> 
#include <string>
#include <fstream>
#include <stdio.h>

#include "MessageTypes.h"

#define LOGFILE "logs.txt"

using namespace std;

class Logger{
public:
	string filename;
	//ofstream loggingFile;
	Logger();
	Logger(string fileName);
	int printTheLog(LogType type, string s);
	//int closeFile();
private:
	int writeToFile(string messages);
};

#endif //LOGGER_H