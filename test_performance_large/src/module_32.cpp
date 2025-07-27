#include "module_32.h"
#include <sstream>
namespace module32 {
  std::string getName() { return "Module 32"; }
  int calculate(int a, int b) { return a + b + 32; }
}
