#include "../inc/Introducer.h"
#include <ctime>
#include <chrono>
Introducer::Introducer()
{
	
}
/**
 * 
 * HeartbeatToNode: Sends a string version of the membership list to the receiving node. The receiving node will convert the string to
 * a <string, long> map where the key is the Addresss (IP + PORT) and value is the heartbeat counter. We then compare the Member.
 * 
 **/
int Introducer::heartbeatToNode(Member destination){
			//The string we send will be seperated line by line --> IP,PORT,HeartbeatCounter 
	string mem_list_to_send = "";
    //Case 1 need to add destination to membership list
    

    //Case 2: destination already exists in the membership list of introducer, just a normal heartbeat
    for(auto& element: this->membershipList){
        pair<string, string> keyPair = element.first;
        pair<int, int> valuePair = element.second;
        mem_list_to_send += keyPair.first + "," + keyPair.second + ","; 
        mem_list_to_send += to_string(valuePair.first) + "\n";
    }
	//Now we have messages ready to send, need to invoke UDP client to send 
	return 0;
}

int Introducer::joinSystem(Member introducer){
	Messages join(JOIN, "IP: " + this->nodeInformation.ip + " PortNum: " + this->nodeInformation.port);
	return 0;
}

int Introducer::listenForHeartbeat(){

}
