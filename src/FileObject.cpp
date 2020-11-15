#include "../inc/FileObject.h"

FileObject::FileObject(string fileName){
    this->fileName = fileName;
    this->checksum = getChecksum();
}

string FileObject::getChecksum(){
    ifstream fileStream(fileName);
    string fileContent((istreambuf_iterator<char>(fileStream)),
                       (istreambuf_iterator<char>()));                       
    size_t hashResult = hash<string>{}(fileContent);
    return to_string(hashResult);
}

string FileObject::toString(){
    return fileName + "::" + checksum;
}

int FileObject::getPositionOnHashring(){
    string toBeHashed = "FILE::" + fileName;
    positionOnHashring = hash<string>{}(toBeHashed) % HASHMODULO;
    return 0;
}
