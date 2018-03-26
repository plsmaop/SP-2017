main: main.cpp
	g++ ./list_file/list_file.cpp -c -o ./list_file/list_file.o
	g++ -std=c++11 ./md5/md5.cpp -c -o ./md5/md5.o -O2
	g++ main.cpp -std=c++11 -O2 -o loser -Wall ./list_file/list_file.o ./md5/md5.o
clean: 
	rm -f loser 
