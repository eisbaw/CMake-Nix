#include "module_255.h"
#include <sstream>
namespace module255 {
  std::string getName() { return "Module 255"; }
  int calculate(int a, int b) { return a + b + 255; }
}
