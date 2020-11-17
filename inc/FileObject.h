#ifndef FILEOBJECT_H
#define FILEOBJECT_H

#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>

#include "HashRing.h"

using namespace std;

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
