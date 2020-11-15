#include "../inc/Node.h"

vector<tuple<string,string, string>> Node::getRandomNodesToGossipTo()
{
    //make an index list, then math.random to get index 
    // from list (remember to exclude the node itself) 
    vector<int> indexList;
    vector<string> IPList;
    vector<string> portList;
    vector<string> timestampList;
    vector<tuple<string, string, string>> selectedNodesInfo;
    //int i = 0;
    for(auto& element: this->membershipList){
        tuple<string, string, string> keyPair = element.first;
        tuple<int, int, int> valueTuple = element.second;
        //cout << "run " << keyPair.first << "/" << keyPair.second << endl;
        //add check to make sure we don't add any failed nodes to gossip to
        if(get<0>(keyPair).compare(this->nodeInformation.ip) != 0 && get<2>(valueTuple) != 1){
            IPList.push_back(get<0>(keyPair));
            portList.push_back(get<1>(keyPair));
            timestampList.push_back(get<2>(keyPair));
            indexList.push_back(IPList.size()-1); // bug fix
        }
        //i++;
    }
    // on one to gossip, return an empty vector
    if (IPList.size() == 0) {
        return selectedNodesInfo;
    }

    /*for (uint j=0; j<indexList.size(); j++) {
        cout << indexList[j] << ":" << IPList[j] << "/" << portList[j] << endl;
    }*/

    switch (this->runningMode) {
        case GOSSIP: {
            srand(time(NULL));
            int nodesSelected = 0;
            // N_b is a predefined number
            if(IPList.size() <= N_b){
                for (uint j=0; j<indexList.size(); j++) {
                    //int chosenIndex = indexList[j];
                    //cout << "put " << IPList[j] << "/" << portList[j] << endl;
                    selectedNodesInfo.push_back(make_tuple(IPList[j], portList[j], timestampList[j]));
                }
            }
            else{
                while (nodesSelected < N_b) {
                    int randomNum = rand() % indexList.size();
                    int chosenIndex = indexList[randomNum];
                    selectedNodesInfo.push_back(make_tuple(IPList[chosenIndex], portList[chosenIndex], timestampList[chosenIndex]));
                    indexList.erase(indexList.begin() + randomNum);
                    nodesSelected++;
                }
            }
            break;
        }
        default: {
            // All2All
            for (uint j=0; j<indexList.size(); j++) {
                //int chosenIndex = indexList[j];
                //cout << "put " << IPList[j] << "/" << portList[j] << endl;
                selectedNodesInfo.push_back(make_tuple(IPList[j], portList[j], timestampList[j]));
            }
            break;
        }
    }
    return selectedNodesInfo;
}