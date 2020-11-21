#ifndef FILEOBJECT_H
#define FILEOBJECT_H

#include <iostream>
#include <string>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fstream>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

#include "HashRing.h"

using namespace std;

string getMostRecentFile(string readfile);

class FileObject {
public:
	string fileName;
	string checksum;
	int positionOnHashring;
	FileObject(string fileName);
	string toString();
    string getChecksum(); //hash the file contents using iterator over whole file
	int getPositionOnHashring();
};

#endif //FILEOBJECT_H
