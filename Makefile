# ================================================================
# SPPS Experiment Suite — Build System
# Target: Linux x86-64 (Ubuntu 22.04+)
# Compiler: g++ -std=c++17 -O3 -march=native
# ================================================================
# NOTE: We bypass pkg-config entirely because the Codespace/system
# pkg-config points to an ancient protobuf 3.12. Our custom 34.0
# is installed in /usr/local/lib.
# ================================================================

CXX      = g++
CXXFLAGS = -std=c++17 -O3 -march=native -I. -I/usr/local/include

# All libraries live in /usr/local/lib (built from source by install_deps_exact.sh)
LDFLAGS  = -L/usr/local/lib

# Protobuf 34.0 + all Abseil dependencies (exhaustive list)
PB_LIBS  = -lprotobuf -lutf8_validity -lutf8_range \
           -labsl_log_internal_check_op -labsl_log_internal_conditions \
           -labsl_log_internal_message -labsl_log_internal_nullguard \
           -labsl_log_internal_log_sink_set -labsl_log_internal_format \
           -labsl_log_internal_structured_proto -labsl_log_internal_proto \
           -labsl_log_internal_fnmatch -labsl_log_internal_globals \
           -labsl_log_sink -labsl_log_entry -labsl_log_globals \
           -labsl_log_initialize -labsl_examine_stack \
           -labsl_die_if_null -labsl_vlog_config_internal \
           -labsl_flags_internal -labsl_flags_marshalling \
           -labsl_flags_reflection -labsl_flags_config \
           -labsl_flags_program_name -labsl_flags_commandlineflag \
           -labsl_flags_commandlineflag_internal \
           -labsl_flags_private_handle_accessor \
           -labsl_raw_hash_set -labsl_hashtablez_sampler \
           -labsl_statusor -labsl_status \
           -labsl_cord -labsl_cordz_info -labsl_cord_internal \
           -labsl_cordz_functions -labsl_cordz_handle \
           -labsl_crc_cord_state -labsl_crc32c -labsl_crc_internal \
           -labsl_crc_cpu_detect \
           -labsl_str_format_internal -labsl_strerror \
           -labsl_synchronization -labsl_graphcycles_internal \
           -labsl_kernel_timeout_internal -labsl_stacktrace \
           -labsl_symbolize -labsl_debugging_internal \
           -labsl_demangle_internal -labsl_demangle_rust \
           -labsl_decode_rust_punycode -labsl_utf8_for_code_point \
           -labsl_malloc_internal -labsl_tracing_internal \
           -labsl_hash -labsl_city -labsl_leak_check \
           -labsl_exponential_biased -labsl_borrowed_fixup_buffer \
           -labsl_random_distributions -labsl_random_seed_sequences \
           -labsl_random_internal_entropy_pool \
           -labsl_random_internal_randen \
           -labsl_random_internal_randen_hwaes \
           -labsl_random_internal_randen_hwaes_impl \
           -labsl_random_internal_randen_slow \
           -labsl_random_internal_platform \
           -labsl_random_internal_seed_material \
           -labsl_random_seed_gen_exception \
           -labsl_time -labsl_civil_time -labsl_time_zone \
           -labsl_strings -labsl_strings_internal -labsl_int128 \
           -labsl_base -labsl_spinlock_wait \
           -labsl_throw_delegate -labsl_raw_logging_internal \
           -labsl_log_severity

# Other libs
LIBS     = -lflatbuffers -lzstd -lpthread -lrt

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
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(PB_SRC) block_c_benchmarks.cpp -o block_c_benchmarks -Wl,--start-group $(PB_LIBS) $(LIBS) -Wl,--end-group
	@echo "Built block_c_benchmarks"

block_d: block_d_louds.cpp $(PB_SRC)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(PB_SRC) block_d_louds.cpp -o block_d_louds -Wl,--start-group $(PB_LIBS) $(LIBS) -Wl,--end-group
	@echo "Built block_d_louds"

block_e: block_e_compression.cpp $(PB_SRC)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(PB_SRC) block_e_compression.cpp -o block_e_compression -Wl,--start-group $(PB_LIBS) $(LIBS) -Wl,--end-group
	@echo "Built block_e_compression"

block_f: block_f_tail_latency.cpp
	$(CXX) $(CXXFLAGS) -o block_f_tail_latency block_f_tail_latency.cpp
	@echo "Built block_f_tail_latency"

block_g: block_g_downstream.cpp $(PB_SRC)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(PB_SRC) block_g_downstream.cpp -o block_g_downstream -Wl,--start-group $(PB_LIBS) -Wl,--end-group
	@echo "Built block_g_downstream"

block_h: block_h_worked_examples.cpp
	$(CXX) $(CXXFLAGS) -o block_h_worked_examples block_h_worked_examples.cpp
	@echo "Built block_h_worked_examples"

profiler: profiler.cpp $(PB_SRC)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(PB_SRC) profiler.cpp -o profiler -Wl,--start-group $(PB_LIBS) $(LIBS) -Wl,--end-group
	@echo "Built profiler"

profiler_isolated: profiler_isolated.cpp $(PB_SRC)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(PB_SRC) profiler_isolated.cpp -o profiler_isolated -Wl,--start-group $(PB_LIBS) $(LIBS) -Wl,--end-group
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
