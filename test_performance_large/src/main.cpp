#include <iostream>
#include <chrono>
#include "module_1.h"
#include "module_2.h"
#include "module_3.h"
#include "module_4.h"
#include "module_5.h"
#include "module_6.h"
#include "module_7.h"
#include "module_8.h"
#include "module_9.h"
#include "module_10.h"
int main() {
  auto start = std::chrono::high_resolution_clock::now();
  int sum = 0;
  sum += module1::calculate(1, 1 * 2);
  sum += module2::calculate(2, 2 * 2);
  sum += module3::calculate(3, 3 * 2);
  sum += module4::calculate(4, 4 * 2);
  sum += module5::calculate(5, 5 * 2);
  sum += module6::calculate(6, 6 * 2);
  sum += module7::calculate(7, 7 * 2);
  sum += module8::calculate(8, 8 * 2);
  sum += module9::calculate(9, 9 * 2);
  sum += module10::calculate(10, 10 * 2);
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  std::cout << "Performance test completed. Sum: " << sum << std::endl;
  std::cout << "Execution time: " << duration.count() << " microseconds" << std::endl;
  return 0;
}
