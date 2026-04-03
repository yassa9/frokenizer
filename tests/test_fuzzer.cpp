// test_fuzzer.cpp
#include <cstdint>
#include <cstddef>
#include <vector>

#include "../include/frokenizer.hpp"

// this is the standard entry point for llvm libfuzzer
// the fuzzer engine will call this function millions of times per second
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) 
{
    frokenizer::Tokenizer tokenizer;

    // in the worst case scenario, every single byte is its own token
    // so we size the output buffer to size + some padding for special tokens
    std::vector<uint32_t> out(size + 128);
    size_t out_len = 0;

    // we only care that it doesn't crash, hang, or read out of bounds
    tokenizer.encode(
        reinterpret_cast<const char*>(data), 
        size, 
        out.data(), 
        out_len, 
        out.size()
    );

    return 0;
}
