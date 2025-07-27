#include "module_256.h"
#include <sstream>
namespace module256 {
  std::string getName() { return "Module 256"; }
  int calculate(int a, int b) { return a + b + 256; }
}
