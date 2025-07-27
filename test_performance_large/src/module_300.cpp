#include "module_300.h"
#include <sstream>
namespace module300 {
  std::string getName() { return "Module 300"; }
  int calculate(int a, int b) { return a + b + 300; }
}
