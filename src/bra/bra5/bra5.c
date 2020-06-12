#include <stdio.h>

int main() {
  int k;
  int a = 0;
  scanf("%d", &k);

  for (int i = 0; i < 20; ++i) {
    a = a + k;
  }
  return a;
}
