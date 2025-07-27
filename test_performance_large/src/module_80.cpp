#include "module_80.h"
#include <sstream>
namespace module80 {
  std::string getName() { return "Module 80"; }
  int calculate(int a, int b) { return a + b + 80; }
}
