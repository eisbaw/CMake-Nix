#include "module_64.h"
#include <sstream>
namespace module64 {
  std::string getName() { return "Module 64"; }
  int calculate(int a, int b) { return a + b + 64; }
}
