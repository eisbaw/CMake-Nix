#include "module_360.h"
#include <sstream>
namespace module360 {
  std::string getName() { return "Module 360"; }
  int calculate(int a, int b) { return a + b + 360; }
}
