#include "../inc/Utils.h"
int main(int argc, char **argv) {
    const char *filename = argv[1];
    std::ifstream file(filename);
    std::string str;
    std::string delim = " ";
    while (std::getline(file, str))
    {
        std::vector<std::string> temp = splitString(str, delim);
        for (auto &e : temp) std::cout << e << "," << "1" << std::endl;
    }
}
