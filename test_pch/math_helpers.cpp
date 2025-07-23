#include "math_helpers.h"

int sum_vector(const std::vector<int>& vec) {
    int sum = 0;
    for (int val : vec) {
        sum += val;
    }
    return sum;
}