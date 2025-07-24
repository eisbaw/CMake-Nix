#ifndef UTILS_H
#define UTILS_H

// Note: Not including vector/iostream because they're in PCH
#include <vector>

void print_vector(const std::vector<int>& vec);
void double_vector(std::vector<int>& vec);

#endif // UTILS_H