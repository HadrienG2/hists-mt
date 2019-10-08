# Technically incorrect, but needed for LTO
CC:=g++
CXX:=g++
LTOFLAGS:=-flto
CXXFLAGS:=-O3 -march=native -std=c++17 $(LTOFLAGS)
LDFLAGS:=$(LTOFLAGS)
LDLIBS:=-pthread -lCore -lHist

TARGETS:=fillBench histConvTests


.PHONY: all clean test

all: $(TARGETS)

clean:
	rm -f $(TARGETS) *.o

test: histConvTests
	./histConvTests

fillBench: fillBench.o
histConvTests: histConvTests.o histConv.o

histConv.o: histConv.hpp
histConvTests.o: histConv.hpp