#include "../inc/Utils.h"
#include <algorithm>

int main(int argc, char **argv) {
    const char *filename = argv[1];
    std::ifstream file(filename);
    std::string str;
    std::string delim = " ";
    while (std::getline(file, str))
    {
        for (size_t i = 0; i < str.size(); i++) {
            if (str[i] == '.' || str[i] == ',' || str[i] == '?' || str[i] == ';' || str[i] == '!' || str[i] == '-') str[i] = ' ';
        }
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        std::vector<std::string> temp = splitString(str, delim);
        for (auto &e : temp) { if (e.size() && e.compare(" ") && e.compare("\n")) std::cout << e << "," << "1" << std::endl; }
    }
}
