#include "read_input_functions.h"

std::string ReadLine() {
    std::string str;
    getline(std::cin, str);
    return str;
}

int ReadInt() {
    int result = 0;
    std::cin >> result;
    return result;
}

int ReadLineWithNumber() {
    int result = 0;
    std::cin >> result;
    ReadLine();
    return result;
}