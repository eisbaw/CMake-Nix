#include "utils.h"

void print_vector(const std::vector<int>& vec) {
    for (int val : vec) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
}

void double_vector(std::vector<int>& vec) {
    for (int& val : vec) {
        val *= 2;
    }
}