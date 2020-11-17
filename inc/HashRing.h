#ifndef HASHRING_H
#define HASHRING_H

#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <vector>
#include <algorithm>
#include "Member.h"

using namespace std;

#define HASHMODULO 360

int hashingId(Member nodeMember, string joinTime);

//Class used for ring leader election algorithm as well as file replication by using successors
class HashRing {
public:
    vector<int> nodePositions; //allow for quick finding of node positions on the ring.
    map<int, string> ring;

    HashRing();
    int locateClosestNode(string filename);
    string getValue(int key); //lookup using ring map
    int addNode(string nodeInfo, int position); // Here we may have to change where other files point to
    int removeNode(int position); // Here we will have to change where all files that point to this node point to
    int getSuccessor(int nodePosition); //Get position of successor node
    int getPredecessor(int nodePosition); //Get position of predecessor node
    void debugHashRing();
    void clear();
    int getRandomNode(tuple<int, int, int> excludedNodes);
};

#endif //HASHRING_H
