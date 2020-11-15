#ifndef HASHRING_H
#define HASHRING_H

#include <iostream> 
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <vector>
#include <algorithm>

using namespace std;

#define HASHMODULO 360

class HashRing {
public:
    vector<int> nodePositions; //allow for quick finding of node positions on the ring. 
    map<int, string> ring;
    map<int, int>  fileToClosestNode; // match up each file to its closest node -> may not need to use this, since we have the locateClosestNode function which maps file position
                                      //to node position on hash ring. 
    
    HashRing();
    vector<string> splitString(string s, string delimiter);
    int locateClosestNode(string filename);
    string getValue(int key);
    int addNode(string nodeInfo, int position); // Here we may have to change where other files point to
    int addFile(string fileInfo, int position);
    int removeNode(int position); // Here we will have to change where all files that point to this node point to
    int removeFile(int position);
    int getSuccessor(int nodePosition); //Get position of successor node 
    int getPredecessor(int nodePosition); //Get position of predecessor node
    void debugHashRing();
    void clear();
    int getRandomNode(tuple<int, int, int> excludedNodes);

};

#endif //HASHRING_H