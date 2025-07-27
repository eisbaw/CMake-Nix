#include <iostream>
int lib_L5_N0_func() {
  int sum = 500;
  extern int lib_L4_N0_func();
  sum += lib_L4_N0_func();
  extern int lib_L4_N1_func();
  sum += lib_L4_N1_func();
  extern int lib_L4_N2_func();
  sum += lib_L4_N2_func();
  extern int lib_L4_N3_func();
  sum += lib_L4_N3_func();
  extern int lib_L4_N4_func();
  sum += lib_L4_N4_func();
  return sum;
}
