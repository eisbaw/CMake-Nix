#include <iostream>
int lib_L1_N0_func() {
  int sum = 100;
  extern int lib_L0_N0_func();
  sum += lib_L0_N0_func();
  extern int lib_L0_N1_func();
  sum += lib_L0_N1_func();
  extern int lib_L0_N2_func();
  sum += lib_L0_N2_func();
  extern int lib_L0_N3_func();
  sum += lib_L0_N3_func();
  extern int lib_L0_N4_func();
  sum += lib_L0_N4_func();
  return sum;
}
