#include <stdio.h>

int fun(int a, int b) {
  int k = a;

  for (; b < 20; ++b) {
    k = a + k;
  }
  return k;
}
