#include "module_404.h"
#include <sstream>
namespace module404 {
  std::string getName() { return "Module 404"; }
  int calculate(int a, int b) { return a + b + 404; }
}
