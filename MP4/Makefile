.PHONY: all clean

# compiler name
CC=g++

# place options like -O2, -g here
CFLAGS= -O2 -g -std=c++11

all: inf-bonbon-server

inf-bonbon-server: inf-bonbon-server.cpp
	$(CC) -c -o child.o $(CFLAGS) child.cpp -ldl
	$(CC) -o inf-bonbon-server $(CFLAGS) inf-bonbon-server.cpp child.o -ldl
	
	

clean:
	rm -f inf-bonbon-server
	rm -f child.o