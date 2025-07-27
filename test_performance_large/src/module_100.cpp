#include "module_100.h"
#include <sstream>
namespace module100 {
  std::string getName() { return "Module 100"; }
  int calculate(int a, int b) { return a + b + 100; }
}
