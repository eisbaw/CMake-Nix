#include "module_400.h"
#include <sstream>
namespace module400 {
  std::string getName() { return "Module 400"; }
  int calculate(int a, int b) { return a + b + 400; }
}
