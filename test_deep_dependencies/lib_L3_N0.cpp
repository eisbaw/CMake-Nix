#include <iostream>
int lib_L3_N0_func() {
  int sum = 300;
  extern int lib_L2_N0_func();
  sum += lib_L2_N0_func();
  extern int lib_L2_N1_func();
  sum += lib_L2_N1_func();
  extern int lib_L2_N2_func();
  sum += lib_L2_N2_func();
  extern int lib_L2_N3_func();
  sum += lib_L2_N3_func();
  extern int lib_L2_N4_func();
  sum += lib_L2_N4_func();
  return sum;
}
