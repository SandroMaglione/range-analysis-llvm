#include <stdio.h>

int fun()
{
    int k = 0;
    int a;

    scanf("%d", &a);

    while (k < 10)
    {
        a = a + 2;
        k = k + 1;
    }

    return a;
}
