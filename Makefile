# Frokenizer Makefile 

# compilers and flags
CXX       ?= g++
CLANGXX   ?= clang++
CXXFLAGS  := -O3 -march=native -Wall -Wextra -Werror -std=c++17 -I./include
OMP_FLAGS := -fopenmp

# sanitizers (for safety tests)
ASAN_FLAGS := -fsanitize=address,undefined -g -O1
FUZZ_FLAGS := -fsanitize=fuzzer,address -g -O2

# directories
BUILD_DIR  := build
INC_DIR    := include
GEN_DIR    := $(INC_DIR)/frokenizer_generated
SRC_DIR    := benchmarks
TEST_DIR   := tests
DATA_DIR   := data
SCRIPT_DIR := scripts

# generated headers
BAKED_HPP := $(GEN_DIR)/baked.hpp
LEXER_HPP := $(GEN_DIR)/lexer.hpp
HEADERS   := $(BAKED_HPP) $(LEXER_HPP) $(INC_DIR)/frokenizer.hpp $(TEST_DIR)/test_utils.hpp

# binaries
BENCH_SINGLE := $(BUILD_DIR)/bench_single
BENCH_MULTI  := $(BUILD_DIR)/bench_multi
TEST_SAFETY  := $(BUILD_DIR)/test_safety
TEST_PARITY  := $(BUILD_DIR)/test_parity
TEST_FUZZER  := $(BUILD_DIR)/test_fuzzer

# data files for tests
TEST_CORPUS  := $(DATA_DIR)/torture.txt
EXPECTED  	 := $(DATA_DIR)/expected.bin
VOCAB     	 := $(DATA_DIR)/qwen.tiktoken

# can be overridden from command line
BENCH_CORPUS ?= $(DATA_DIR)/news.2013.en.shuffled

.PHONY: all generate test test-safety test-parity bench clean fuzz data

# default target
all: generate $(TEST_SAFETY) $(TEST_PARITY) $(BENCH_SINGLE) $(BENCH_MULTI)

# ---------------------------------------------------------
# generation rules (Lexer & Baker)
# ---------------------------------------------------------
generate: $(BAKED_HPP) $(LEXER_HPP)

$(BAKED_HPP): $(VOCAB) $(SCRIPT_DIR)/baker.py
	@echo "[*] baking static vocabulary tree ..."
	@mkdir -p $(GEN_DIR)
	python3 $(SCRIPT_DIR)/baker.py $(VOCAB) $(BAKED_HPP)

$(LEXER_HPP): $(SCRIPT_DIR)/lexer.re.hpp
	@echo "[*] compiling regex dfa with re2c ..."
	@mkdir -p $(GEN_DIR)
	re2c -8 $(SCRIPT_DIR)/lexer.re.hpp -o $(LEXER_HPP)

# ---------------------------------------------------------
# data generation
# ---------------------------------------------------------
data: $(TEST_CORPUS) $(EXPECTED)
 
$(TEST_CORPUS): $(TEST_DIR)/gen_stubborn.py
	@mkdir -p $(DATA_DIR)
	@echo "[*] generating torture corpus ..."
	python3 $(TEST_DIR)/gen_stubborn.py $@

$(EXPECTED): $(TEST_CORPUS) $(VOCAB) $(TEST_DIR)/gen_gt.py
	@echo "[*] generating ground truth binaries ..."
	python3 $(TEST_DIR)/gen_gt.py $(TEST_CORPUS) $(VOCAB) $@

$(BENCH_CORPUS): benchmarks/download_wmt.sh
	@mkdir -p $(DATA_DIR)
	@bash benchmarks/download_wmt.sh $@

# ---------------------------------------------------------
# tests
# ---------------------------------------------------------
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TEST_SAFETY): $(TEST_DIR)/test_safety.cpp $(HEADERS) | $(BUILD_DIR)
	@echo "[*] compiling safety suite (ASAN/UBSAN) ..."
	$(CXX) $(CXXFLAGS) $(ASAN_FLAGS) $< -o $@

$(TEST_PARITY): $(TEST_DIR)/test_parity.cpp $(HEADERS) | $(BUILD_DIR)
	@echo "[*] compiling parity test ..."
	$(CXX) $(CXXFLAGS) $< -o $@

test-safety: $(TEST_SAFETY)
	./$(TEST_SAFETY)

test-parity: $(TEST_PARITY) data
	./$(TEST_PARITY) $(TEST_CORPUS) $(EXPECTED)

test: test-safety test-parity

# ---------------------------------------------------------
# fuzzer (requires clang)
# ---------------------------------------------------------
$(TEST_FUZZER): $(TEST_DIR)/test_fuzzer.cpp $(HEADERS) | $(BUILD_DIR)
	@echo "[*] compiling fuzzer ..."
	$(CLANGXX) -std=c++17 -I./include $(FUZZ_FLAGS) $< -o $@

fuzz: $(TEST_FUZZER)
	@echo "[*] running fuzzer (press ctrl+c to stop) ..."
	./$(TEST_FUZZER) -max_total_time=60

# ---------------------------------------------------------
# benchmarks
# ---------------------------------------------------------
$(BENCH_SINGLE): $(SRC_DIR)/bench_single.cpp $(HEADERS) | $(BUILD_DIR)
	@echo "[*] compiling single-core benchmark ..."
	$(CXX) $(CXXFLAGS) $< -o $@

$(BENCH_MULTI): $(SRC_DIR)/bench_multi.cpp $(HEADERS) | $(BUILD_DIR)
	@echo "[*] compiling multi-core benchmark ..."
	$(CXX) $(CXXFLAGS) $(OMP_FLAGS) $< -o $@

bench: $(BENCH_SINGLE) $(BENCH_MULTI) $(BENCH_CORPUS)
	@echo "\n[*] --- RUNNING SINGLE-CORE BENCHMARK ---"
	python3 $(SRC_DIR)/bench_single.py $(BENCH_SINGLE) $(BENCH_CORPUS) $(VOCAB)
	@echo "\n[*] --- RUNNING MULTI-CORE SCALING BENCHMARK ---"
	python3 $(SRC_DIR)/bench_multi.py $(BENCH_MULTI) $(BENCH_CORPUS) $(VOCAB)

# ---------------------------------------------------------
# cleanup
# ---------------------------------------------------------
clean:
	@echo "[*] cleaning build directory ..."
	@echo "[*] cleaning include/frokenizer_generated directory ..."
	rm -rf $(BUILD_DIR) $(GEN_DIR)
