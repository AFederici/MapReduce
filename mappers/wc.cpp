#include "../inc/Utils.h"
int main(int argc, char **argv) {
    const char *filename = argv[1];
    std::ifstream file(filename);
    std::string str;
    std::string delim = " ";
    while (std::getline(file, str))
    {
        for (int i = 0; i < str.size(); i++) {
            if (str[i] == '.' || str[i] == ',' || str[i] == '?' || str[i] == ';' || str[i] == '!') str[i] = ' ';
            str[i] = tolower(str[i]);
        }
        std::vector<std::string> temp = splitString(str, delim);
        for (auto &e : temp) std::cout << e << "," << "1" << std::endl;
    }
}
