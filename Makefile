# Technically incorrect, but needed for LTO
CC:=g++
CXX:=g++
CXXFLAGS:=-O3 -march=native -std=c++17 -flto
LDFLAGS:=-flto
LDLIBS:=-pthread -lCore -lHist

TARGETS:=fillBench histConvTests


.PHONY: all clean test

all: $(TARGETS)

clean:
	rm -f $(TARGETS) *.o

test: histConvTests
	./histConvTests

fillBench.exe: fillBench.o
histConvTests.exe: histConvTests.o

fillBench.o: histConv.hpp