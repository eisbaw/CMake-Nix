#include "module_2.h"
#include <sstream>
namespace module2 {
  std::string getName() { return "Module 2"; }
  int calculate(int a, int b) { return a + b + 2; }
}
