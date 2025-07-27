#include "module_200.h"
#include <sstream>
namespace module200 {
  std::string getName() { return "Module 200"; }
  int calculate(int a, int b) { return a + b + 200; }
}
