
CXX = g++
CFLAGS = -Wall -g -O3

all:
	$(CXX) $(CFLAGS) execute.cpp -o execute

clean:
	rm datablock btree execute
