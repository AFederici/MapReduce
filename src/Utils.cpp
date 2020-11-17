#include "../inc/Utils.h"

vector<string> splitString(string s, string delimiter){
	vector<string> result;
	size_t pos_start = 0, pos_end, delim_len = delimiter.length();
	string token;
	while ((pos_end = s.find (delimiter, pos_start)) != string::npos) {
		token = s.substr (pos_start, pos_end - pos_start);
		pos_start = pos_end + delim_len;
		result.push_back(token);
	}
	result.push_back (s.substr (pos_start));
	return result;
}

string getIP(){
	char host[100] = {0};
	if (gethostname(host, sizeof(host)) < 0) {
		perror("error: gethostname");
	}
	return getIP(host);
}

string getIP(const char * host){
	struct hostent *hp;
	if (!(hp = gethostbyname(host)) || (hp->h_addr_list[0] == NULL)) {
		perror("error: no ip");
		exit(1);
	}
	return inet_ntoa(*(struct in_addr*)hp->h_addr_list[0]);
}

int new_thread_id() {
    int rv;
    pthread_mutex_lock(&thread_counter_lock);
    rv = ++thread_counter;
    pthread_mutex_unlock(&thread_counter_lock);
    return rv;
}

bool isInVector(vector<int> v, int i){
	for(int element: v){
		if(element == i){
			return true;
		}
	}
	return false;
}
