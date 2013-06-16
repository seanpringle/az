all:
	gcc -Wall -Os -g -o az az.c && ./az test.az