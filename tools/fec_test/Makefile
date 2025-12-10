# Simple Makefile for FEC Decoder Test Tool
# Usage: make

CXX ?= g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
INCLUDES = -I. -I../../components/common -I../../components/gs_fec_decoder/include

SOURCES = main.cpp pcap_parser.cpp ../../components/common/fec.cpp
TARGET = fec_test

.PHONY: all clean test unit_test

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

unit_test: unit_test.cpp ../../components/common/fec.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

clean:
	rm -f $(TARGET) unit_test

# Test with a sample PCAP file (if available)
test: $(TARGET)
	@echo "Run with: ./fec_test <pcap_file>"
	@echo "Expected output for colorbars capture:"
	@echo "  Packets processed: 11406"
	@echo "  Primary packets: 8542"
	@echo "  FEC packets: 2864"
	@echo "  FEC blocks complete: 3233"
	@echo "  FEC blocks recovered: 664"
	@echo "  FEC blocks failed: 23"
	@echo "  Frames decoded: 481"
	@echo "  Frames incomplete: 78"

help:
	@echo "FEC Decoder Test Tool"
	@echo "====================="
	@echo "Build: make"
	@echo "Clean: make clean"
	@echo "Usage: ./fec_test <pcap_file> [options]"
	@echo ""
	@echo "Options:"
	@echo "  -k <num>    FEC coding_k (default: 6)"
	@echo "  -n <num>    FEC coding_n (default: 12)"
	@echo "  -m <num>    MTU size (default: 1464)"
	@echo "  -v          Verbose output"
