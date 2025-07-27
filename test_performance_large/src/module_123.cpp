#include "module_123.h"
#include <sstream>
namespace module123 {
  std::string getName() { return "Module 123"; }
  int calculate(int a, int b) { return a + b + 123; }
}
