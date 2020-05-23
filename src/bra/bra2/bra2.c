#include <stdio.h>

int main() {
  int k = 0;
  while (k < 100) {
    int i = 0;
    int j = k;
    if (i < j) {
      i = i + 1;
      j = j - 1;
    }
    k = k + 1;
  }
  return k;
}
