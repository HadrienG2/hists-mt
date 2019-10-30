# CC=g++ is technically incorrect, but needed for LTO w/ implicit make rules
CC:=g++
CXX:=g++
LTOFLAGS:=-flto=$(shell nproc --all)
CXXFLAGS:=-O3 -march=native -std=c++17 -Wall -Wextra -pedantic $(LTOFLAGS)
LDFLAGS:=$(LTOFLAGS)
LDLIBS:=-pthread -lCore -lHist

TARGETS:=fillBench histConvTests


.PHONY: all bench clean test

all: $(TARGETS)

bench: fillBench
	./fillBench

clean:
	rm -f $(TARGETS) *.o

test: histConvTests
	./histConvTests


fillBench: fillBench.o
histConvTests: histConvTests.o histConv.o histConvTests_exotic_stats.o \
			   histConvTests_utilities.o

histConv.o: histConv.hpp histConv.hpp.dcl
histConvTests.o: histConv.hpp.dcl histConvTests.hpp histConvTests.hpp.dcl
histConvTests_exotic_stats.o: histConv.hpp histConv.hpp.dcl histConvTests.hpp \
							  histConvTests.hpp.dcl
histConvTests_utilities.o: histConvTests.hpp.dcl
