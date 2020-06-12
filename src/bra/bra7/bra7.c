#include <stdio.h>

int fun() {
  int a;
  int b;
  int k = 1;
  
  scanf("%d", &a);
  scanf("%d", &b);

  while (a < 10) {
    if (a < b) {
      k = k + 1;
    } else {
      k = k + 2;
    }
  }
  return k;
}
