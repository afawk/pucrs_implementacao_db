
CXX = g++
CFLAGS = -Wall -g -O3

all:
	$(CXX) $(CFLAGS) datablock.c -o datablock
	$(CXX) $(CFLAGS) btree.cpp -o btree

clean:
	rm datablock btree
