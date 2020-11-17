#ifndef UTILS_H
#define UTILS_H
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#ifdef __linux__
#include <bits/stdc++.h>
#endif

using std::string;
using std::vector;
using std::get;
using std::tuple_element;

static pthread_mutex_t thread_counter_lock = PTHREAD_MUTEX_INITIALIZER;
static int thread_counter = 0;

vector<string> splitString(string s, string delimiter);
string getIP();
string getIP(const char * host);
int new_thread_id();
bool isInVector(vector<int> v, int i);
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

#endif //UDPSOCKET_H
