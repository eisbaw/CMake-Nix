#include "module_500.h"
#include <sstream>
namespace module500 {
  std::string getName() { return "Module 500"; }
  int calculate(int a, int b) { return a + b + 500; }
}
