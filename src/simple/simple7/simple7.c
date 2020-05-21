#include <stdio.h>

int main() {
	int a = 0;
	int b = 10;
	int c = 3 + a;
	a = b - c;
	c = a + 7 + 3;
	b = b + b - 1 - a;
	return 0;
}
