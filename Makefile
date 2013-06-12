all:
	gcc -Wall -O2 -o az az.c && ./az test.az