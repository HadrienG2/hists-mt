# CC=g++ is technically incorrect, but needed for LTO
CC:=g++
CXX:=g++
LTOFLAGS:=-flto
CXXFLAGS:=-O3 -march=native -std=c++17 $(LTOFLAGS)
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

histConv.o: histConv.hpp
histConvTests.o: histConv.hpp.dcl histConvTests.hpp
histConvTests_exotic_stats.o: histConv.hpp histConvTests.hpp
histConvTests_utilities.o: histConvTests.hpp

histConv.hpp: histConv.hpp.dcl