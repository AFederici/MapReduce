#include "../inc/HashRing.h"

HashRing::HashRing(){}

string HashRing::getValue(int key){
    if(key == -1){
        return "No node found";
    }
    return ring[key];
}

void HashRing::clear()
{
    ring.clear();
    nodePositions.clear();
}

int hashingId(Member nodeMember, string joinTime)
{
	string toBeHashed = "NODE::" + nodeMember.ip + "::" + nodeMember.port + "::" + joinTime;
	int ringPosition = hash<string>{}(toBeHashed) % HASHMODULO;
	return ringPosition;
}

void HashRing::debugHashRing()
{
    cout << "Current Ring: " << endl;
    for (auto& element: ring) {
        int position = element.first;
        string object = element.second;
        cout << object << " at " << position << endl;
    }
}

int HashRing::addNode(string nodeInfo, int position){
    ring[position] = nodeInfo;
    nodePositions.push_back(position);
    sort(nodePositions.begin(), nodePositions.end());
    return 0;
    //TODO: deal with hash collisions?
}

int HashRing::removeNode(int position){
    ring.erase(position);
    for(uint i = 0; i < nodePositions.size(); i++){
        if(nodePositions[i] == position){
            nodePositions.erase(nodePositions.begin() + i);
        }
    }
    return 0;
}

int HashRing::locateClosestNode(string filename){
    int filePosition;
    filePosition = hash<string>{}(filename) % HASHMODULO;
    for(int i : nodePositions){
        if(i >= filePosition){
            return i;
        }
    }
    //If we cannnot find a Node at a entry on the hash circle greater than or equal to our file position, we wrap back around and take
    // the first node's position as where that file should go.
    return nodePositions[0];
}

int HashRing::getPredecessor(int nodePosition){
    if(nodePositions.size() == 1){
        return nodePosition;
    }
    unsigned int indexOfNode = -1;
    for(uint i = 0; i < nodePositions.size(); i++){
        if(nodePositions[i] == nodePosition){
            indexOfNode = i;
            break;
        }
    }
    //If indexOfNode = 0, get the last entry in Node position as predecessor
    if(indexOfNode == 0){
        return nodePositions[nodePositions.size() - 1];
    }
    return nodePositions[indexOfNode - 1];
}

int HashRing::getSuccessor(int nodePosition){
    if(nodePositions.size() == 1){
        return nodePosition;
    }
    unsigned int indexOfNode = -1;
    for(uint i = 0; i < nodePositions.size(); i++){
        if(nodePositions[i] == nodePosition){
            indexOfNode = i;
            break;
        }
    }
    //If indexOfNode = size of nodePosition - 1, get the first entry in Node position as successor
    if(indexOfNode == nodePositions.size() - 1){
        return nodePositions[0];
    }
    return nodePositions[indexOfNode + 1];
}


//exclude the three positions in the tuple,
//return node position in hash ring of a random node that is not one of the
//three nodes in this tuple.
int HashRing::getRandomNode(tuple<int, int, int> excludedNodes){
    if(nodePositions.size() >= 4){
        //get random node
        // return that random node
        vector<int> indicesToPickFrom;
        for(unsigned int i = 0; i < nodePositions.size(); i++){
            if(nodePositions[i] != get<0>(excludedNodes) && nodePositions[i] != get<1>(excludedNodes) && nodePositions[i] != get<2>(excludedNodes)){
                indicesToPickFrom.push_back(i);
            }
        }
        int randomSelection = rand() % indicesToPickFrom.size();
        return nodePositions[indicesToPickFrom[randomSelection]];
    }
    return -1;
}

int HashRing::size() { return nodePositions.size(); }
