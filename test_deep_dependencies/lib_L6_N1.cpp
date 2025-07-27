#include <iostream>
int lib_L6_N1_func() {
  int sum = 601;
  extern int lib_L5_N0_func();
  sum += lib_L5_N0_func();
  extern int lib_L5_N1_func();
  sum += lib_L5_N1_func();
  extern int lib_L5_N2_func();
  sum += lib_L5_N2_func();
  extern int lib_L5_N3_func();
  sum += lib_L5_N3_func();
  extern int lib_L5_N4_func();
  sum += lib_L5_N4_func();
  return sum;
}
