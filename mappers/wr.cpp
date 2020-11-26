#include "../inc/Utils.h"
#include <algorithm>

int main(int argc, char **argv) {
    const char *filename = argv[1];
    std::ifstream file(filename);
    std::string str, key;
    std::string delim = ",";
    int counter = 0;
    while (std::getline(file, str))
    {
        std::vector<std::string> temp = splitString(str, delim);
        key = temp[0];
        try { counter += stoi(temp[1]); }
        catch(std::invalid_argument& e){ continue; }
    }
    std::cout << key << "," << std::to_string(counter) << std::endl;
}
