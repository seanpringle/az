.PHONY: all az

all: az

az:
	gcc -Wall -Os -g -o az az.c && objdump -d az > az.dump && ./az test.az

clean:
	rm -f az