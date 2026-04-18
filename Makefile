# ================================================================
# SPPS Experiment Suite — Build System
# Target: Linux x86-64 (Ubuntu 22.04, AMD Ryzen 5 7235HS)
# Compiler: g++ -std=c++17 -O3 -march=native
# ================================================================
# SPPS Experiment Suite — Makefile
# Platform: Linux x86-64 (Ubuntu 22.04)
# Compiler: g++ -std=c++17 -O3 -march=native
# Profiling: perf stat + AMD uProf (for AMD Ryzen)

CXX      = g++
CXXFLAGS = -std=c++17 -O3 -march=native -I.
PB_INC   = $(shell pkg-config --cflags protobuf)
PB_LIBS  = $(shell pkg-config --libs protobuf)
LIBS     = -lprotobuf-lite -lflatbuffers -lzstd -lpthread

# Generated protobuf source (run: protoc --cpp_out=. tree.proto)
PB_SRC   = tree.pb.cc

.PHONY: all clean block_a block_b block_c block_d block_e block_f block_g block_h profiler profiler_isolated gen-proto

all: block_a block_b block_c block_d block_e block_f block_g block_h profiler profiler_isolated

block_a: block_a_correctness.cpp
	$(CXX) $(CXXFLAGS) -o block_a_correctness block_a_correctness.cpp
	@echo "Built block_a_correctness"

block_b: block_b_linearity.cpp
	$(CXX) $(CXXFLAGS) -o block_b_linearity block_b_linearity.cpp
	@echo "Built block_b_linearity"

block_c: block_c_benchmarks.cpp $(PB_SRC)
	$(CXX) $(CXXFLAGS) $(PB_INC) $(PB_SRC) block_c_benchmarks.cpp -o block_c_benchmarks -Wl,--start-group -Wl,-Bstatic $(PB_LIBS) -Wl,-Bdynamic -Wl,--end-group $(LIBS)
	@echo "Built block_c_benchmarks"

block_d: block_d_louds.cpp $(PB_SRC)
	$(CXX) $(CXXFLAGS) $(PB_INC) $(PB_SRC) block_d_louds.cpp -o block_d_louds -Wl,--start-group -Wl,-Bstatic $(PB_LIBS) -Wl,-Bdynamic -Wl,--end-group $(LIBS)
	@echo "Built block_d_louds"

block_e: block_e_compression.cpp $(PB_SRC)
	$(CXX) $(CXXFLAGS) $(PB_INC) $(PB_SRC) block_e_compression.cpp -o block_e_compression -Wl,--start-group -Wl,-Bstatic $(PB_LIBS) -Wl,-Bdynamic -Wl,--end-group $(LIBS)
	@echo "Built block_e_compression"

block_f: block_f_tail_latency.cpp
	$(CXX) $(CXXFLAGS) -o block_f_tail_latency block_f_tail_latency.cpp
	@echo "Built block_f_tail_latency"

block_g: block_g_downstream.cpp $(PB_SRC)
	$(CXX) $(CXXFLAGS) $(PB_INC) $(PB_SRC) block_g_downstream.cpp -o block_g_downstream -Wl,--start-group -Wl,-Bstatic $(PB_LIBS) -Wl,-Bdynamic -Wl,--end-group
	@echo "Built block_g_downstream"

block_h: block_h_worked_examples.cpp
	$(CXX) $(CXXFLAGS) -o block_h_worked_examples block_h_worked_examples.cpp
	@echo "Built block_h_worked_examples"

profiler: profiler.cpp $(PB_SRC)
	$(CXX) $(CXXFLAGS) $(PB_INC) $(PB_SRC) profiler.cpp -o profiler -Wl,--start-group -Wl,-Bstatic $(PB_LIBS) -Wl,-Bdynamic -Wl,--end-group $(LIBS)
	@echo "Built profiler"

profiler_isolated: profiler_isolated.cpp $(PB_SRC)
	$(CXX) $(CXXFLAGS) $(PB_INC) $(PB_SRC) profiler_isolated.cpp -o profiler_isolated -Wl,--start-group -Wl,-Bstatic $(PB_LIBS) -Wl,-Bdynamic -Wl,--end-group $(LIBS)
	@echo "Built profiler_isolated"

clean:
	rm -f block_a_correctness block_b_linearity block_c_benchmarks \
	      block_d_louds block_e_compression block_f_tail_latency \
	      block_g_downstream block_h_worked_examples profiler profiler_isolated
	rm -f *.o
	@echo "Cleaned build artifacts"

# Regenerate protobuf and flatbuffers headers from schema files
gen-proto:
	protoc --cpp_out=. tree.proto
	flatc --cpp tree.fbs
	@echo "Generated tree.pb.h tree.pb.cc tree_generated.h"
