CXX=g++
CXXFLAGS=-std=c++11 -Wall -pedantic -g
LD=g++
LDFLAGS=-g -Wall -pedantic
LIBS=-lpthread


all: test

test: solution.o main.o
	$(LD) $(LDFLAGS) $^ -o $@ $(LIBS)

	
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
	
clean:
	rm -f *.o test
	
clear: clean
	rm -f core *.bak *~ *.o

solution.o: solution.cpp common.h
main.o: main.cpp common.h 
