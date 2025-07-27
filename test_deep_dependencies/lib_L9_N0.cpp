#include <iostream>
int lib_L9_N0_func() {
  int sum = 900;
  extern int lib_L8_N0_func();
  sum += lib_L8_N0_func();
  extern int lib_L8_N1_func();
  sum += lib_L8_N1_func();
  extern int lib_L8_N2_func();
  sum += lib_L8_N2_func();
  extern int lib_L8_N3_func();
  sum += lib_L8_N3_func();
  extern int lib_L8_N4_func();
  sum += lib_L8_N4_func();
  return sum;
}
