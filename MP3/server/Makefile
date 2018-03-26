.PHONY: all clean

# compiler name
CC=g++

# place options like -O2, -g here
CFLAGS= -O2 -g -std=c++11

all: boss

boss: boss.cpp 
	$(CC) -o boss $(CFLAGS)  boss.cpp  -lssl -lcrypto

clean:
	rm -f boss