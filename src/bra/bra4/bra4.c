#include <stdio.h>

int main() {
  int k = 0;
  while (k < 30) {
    int i = 10;
    int j = k;
    if (i < 5 && i > -5) {
      i = i + 1;
    }
    k = k + 2;
  }
  return k;
}
