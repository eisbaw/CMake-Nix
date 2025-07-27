#include <iostream>
int lib_L7_N2_func() {
  int sum = 702;
  extern int lib_L6_N0_func();
  sum += lib_L6_N0_func();
  extern int lib_L6_N1_func();
  sum += lib_L6_N1_func();
  extern int lib_L6_N2_func();
  sum += lib_L6_N2_func();
  extern int lib_L6_N3_func();
  sum += lib_L6_N3_func();
  extern int lib_L6_N4_func();
  sum += lib_L6_N4_func();
  return sum;
}
