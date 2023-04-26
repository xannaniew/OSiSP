CC=g++

all: parent child

parent: parent.o
	$(CC) parent.o -g -o parent

parent.o: parent.cpp
	$(CC) -c parent.cpp

child: child.o
	$(CC) child.o -g -o child

child.o: child.cpp
	$(CC) -c child.cpp
