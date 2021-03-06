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

string getMostRecentFile(string readfile){
    struct dirent *entry = nullptr;
    DIR *dp = nullptr;
    int matchLen = readfile.size();
    vector<string> fileVersions;
    if ((dp = opendir(".")) == nullptr) { cout << "temp directory error " << endl; closedir(dp); return ""; }
    while ((entry = readdir(dp))){
        if (strncmp(entry->d_name, readfile.c_str(), matchLen) == 0){
            fileVersions.push_back(entry->d_name);
        }
    }
    sort(fileVersions.begin(), fileVersions.end());
    closedir(dp);
    return fileVersions[fileVersions.size()-1];
}

void cleanupTmpFiles(string match){
    struct dirent *entry = nullptr;
    DIR *dp = nullptr;
    int matchLen = match.size();
    int counter = 0;
    if ((dp = opendir(".")) == nullptr) { cout << "tmp directory error " << endl;}
    while ((entry = readdir(dp))){
        if (strncmp(entry->d_name, match.c_str(), matchLen) == 0){
            if (remove(entry->d_name) == 0) counter++;
        }
    }
    cout << "[CLEANUP] remove: " << counter << " tmp- files" << endl;
    closedir(dp);
}
