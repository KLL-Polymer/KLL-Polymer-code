CXX = g++-11
CXXFLAGS = -O3 -std=c++20 -frtti -fno-rtti

ROCKSDB_DIR = ./rocksdb
KLL_DIR = ./algorithm

INCLUDES = -I$(ROCKSDB_DIR)/include -I.

LIB_PATH = -L$(ROCKSDB_DIR)

LIBS = -lrocksdb -lpthread -lsnappy -lz -lbz2 -llz4 -lzstd -ldl

TARGET = kll_bench
SRC = benchmark.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) -o $(TARGET) $(LIB_PATH) $(LIBS)

clean:
	rm -f $(TARGET)
	rm -rf /tmp/rocksdb_kll_final_benchmark

.PHONY: all clean