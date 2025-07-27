#include "module_16.h"
#include <sstream>
namespace module16 {
  std::string getName() { return "Module 16"; }
  int calculate(int a, int b) { return a + b + 16; }
}
