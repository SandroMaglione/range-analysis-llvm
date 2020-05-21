#include <stdio.h>

int main() {
  int k = 0;
  int j = k;

  if (k < 100) {
    k = k + 1;
    j = k;
  } else {
    j = j - 1;
    k = j;
  }

  return k;
}
