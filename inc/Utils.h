#ifndef UTILS_H
#define UTILS_H
#include <iostream>
#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#ifdef __linux__
#include <bits/stdc++.h>
#endif

using std::string;
using std::cout;
using std::endl;
using std::vector;
using std::get;
using std::tuple_element;
using std::tuple;

static pthread_mutex_t thread_counter_lock = PTHREAD_MUTEX_INITIALIZER;
static int thread_counter = 0;

vector<string> splitString(string s, string delimiter);
string getIP();
string getIP(const char * host);
int new_thread_id();
void handlePipe(int file, string prefix);
bool isInVector(vector<int> v, int i);

void sigchld_handler(int s);

//adapted from https://stackoverflow.com/questions/23030267/custom-sorting-a-vector-of-tuples
template<int M, template<typename> class F = std::less>
struct TupleCompare
{
    template<typename T>
    bool operator()(T const &t1, T const &t2)
    {
        return F<typename tuple_element<M, T>::type>()(std::get<M>(t1), std::get<M>(t2));
    }
};

template<typename T>
vector<T> randItems(int numItems, vector<T> toChoose){
	srand(time(NULL));
	vector<T> selectedNodesInfo;
	vector<int> indexList;
	int availableNodes = toChoose.size();
	for (int i = 0; i < availableNodes; i++) indexList.push_back(i);
	if (availableNodes <= numItems) return toChoose;
	int nodeCount = 0;
	while (nodeCount < numItems) {
		int randomNum = rand() % availableNodes;
		selectedNodesInfo.push_back(toChoose[indexList[randomNum]]);
		indexList.erase(indexList.begin() + randomNum);
		availableNodes--;
		nodeCount++;
	}
	return selectedNodesInfo;
}

#endif //UDPSOCKET_H
