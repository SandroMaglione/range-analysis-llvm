#include <stdio.h>

int main() {
  int k = 0;
  int a = 1;

  while(k < 20) {
    for (int j = 0; j < 10; j++) {
      a = a + 3;
    }
    k = k + 1;
  }
  printf ("%d", k);

  return a;
}
