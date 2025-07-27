#include <iostream>
int lib_L2_N1_func() {
  int sum = 201;
  extern int lib_L1_N0_func();
  sum += lib_L1_N0_func();
  extern int lib_L1_N1_func();
  sum += lib_L1_N1_func();
  extern int lib_L1_N2_func();
  sum += lib_L1_N2_func();
  extern int lib_L1_N3_func();
  sum += lib_L1_N3_func();
  extern int lib_L1_N4_func();
  sum += lib_L1_N4_func();
  return sum;
}
