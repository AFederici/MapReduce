#include "../inc/HashRing.h"


HashRing::HashRing(){

}

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

void HashRing::debugHashRing()
{
    cout << "Current Ring: " << endl;
    for (auto& element: ring) { 
        int position = element.first;
        string object = element.second;
        cout << object << " at " << position << endl;
    }
}

vector<string> HashRing::splitString(string s, string delimiter){
	vector<string> result;
	size_t pos_start = 0, pos_end, delim_len = delimiter.length();
	string token;

	while ((pos_end = s.find (delimiter, pos_start)) != string::npos) {
		token = s.substr (pos_start, pos_end - pos_start);
		pos_start = pos_end + delim_len;
		result.push_back (token);
	}

	result.push_back (s.substr (pos_start));
	return result;
}

int HashRing::addFile(string fileInfo, int position){
    ring[position] = fileInfo;
    fileToClosestNode[position] = locateClosestNode(fileInfo);
    //TODO: deal with hash collisions?
    return 0;
}

int HashRing::addNode(string nodeInfo, int position){
    ring[position] = nodeInfo;
    nodePositions.push_back(position); //Add node position on hash ring to list of node positions
    sort(nodePositions.begin(), nodePositions.end());
    //int insertPosition = -1;
    //for(uint i = 0; i < nodePositions.size(); i++){
    //   if(nodePositions[i] == position){
    //        insertPosition = i; 
    //        break;
    //    }
    //}
    /* commenting out this code since we are making hash ring node only 
    //Case where the newly inserted node is not at beginning of list
    if(insertPosition != 0){
        for(map<int, int>::iterator it = fileToClosestNode.begin(); it != fileToClosestNode.end(); it++){
            if(it -> first <= nodePositions[insertPosition] && it -> first > nodePositions[insertPosition - 1]){
                it -> second = locateClosestNode(it -> first);
            }
        }
    }
    //If newly inserted not is at beginning of list, just need to look at files with indexes either less than that node's index,
    //or at files with index greater than the highest node index
    else{
        for(map<int, int>::iterator it = fileToClosestNode.begin(); it != fileToClosestNode.end(); it++){
            if(it -> first <= nodePositions[insertPosition] || it -> first > nodePositions[nodePositions.size() -1]){
                it -> second = locateClosestNode(it -> first);
            }
        }
    }
    */
    return 0;
    //TODO: deal with hash collisions?
}

int HashRing::removeFile(int position){
    ring.erase(position);
    fileToClosestNode.erase(position);
    return 0;

}

int HashRing::removeNode(int position){
    ring.erase(position);
    for(uint i = 0; i < nodePositions.size(); i++){
        if(nodePositions[i] == position){
            nodePositions.erase(nodePositions.begin() + i);
        }
    }
    //find all files that point to this node, and tell them to now point to their next closest node instead. 
    /* comment out this code since we are making hash ring node only at this point. 
    for(map<int, int>::iterator it = fileToClosestNode.begin(); it != fileToClosestNode.end(); it ++){
        if(it ->second == position){
            it ->second = locateClosestNode(it ->first);
        }
    }*/

    return 0;
}

int HashRing::locateClosestNode(string filename){
    int filePosition;
    //Hash filename to get file position on hash ring, then use that to find closest node to it
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