.PHONY: all clean

# compiler name
CC=g++

# place options like -O2, -g here
CFLAGS= -O2 -g -std=c++11

all: miner

miner: miner.cpp
	$(CC) -o miner $(CFLAGS) miner.cpp  -lssl -lcrypto
	

clean:
	rm -f miner