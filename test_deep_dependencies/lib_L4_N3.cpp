#include <iostream>
int lib_L4_N3_func() {
  int sum = 403;
  extern int lib_L3_N0_func();
  sum += lib_L3_N0_func();
  extern int lib_L3_N1_func();
  sum += lib_L3_N1_func();
  extern int lib_L3_N2_func();
  sum += lib_L3_N2_func();
  extern int lib_L3_N3_func();
  sum += lib_L3_N3_func();
  extern int lib_L3_N4_func();
  sum += lib_L3_N4_func();
  return sum;
}
