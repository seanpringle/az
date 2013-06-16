all:
	gcc -Wall -Os -o az az.c && ./az test.az