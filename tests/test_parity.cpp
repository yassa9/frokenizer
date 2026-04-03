// test_parity.cpp
// Copyright (c) 2026 Yassa Sfen
// SPDX-License-Identifier: MIT

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "../include/frokenizer.hpp"
#include "test_utils.hpp"

std::vector<char> read_file(const char* path) 
{
    FILE* f = fopen(path, "rb");
    ASSERT(f != nullptr, "failed to open file");

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    std::vector<char> buf(len);
    size_t read_bytes = fread(buf.data(), 1, len, f);
    fclose(f);

    ASSERT(read_bytes == (size_t)len, "failed to read all bytes from file");
    return buf;
}

void run_parity_test(const char* corpus_path, const char* expected_path) 
{
    LOG_INFO("loading corpus and ground truth binary ...");
    std::vector<char> corpus_data = read_file(corpus_path);
    std::vector<char> expected_data = read_file(expected_path);

    size_t expected_token_count = expected_data.size() / sizeof(uint32_t);
    uint32_t* expected_tokens = reinterpret_cast<uint32_t*>(expected_data.data());

    LOG_INFO("expected token count: %zu", expected_token_count);

    // allocate safe padding for actual tokens
    std::vector<uint32_t> actual_tokens(expected_token_count + 1024);
    size_t actual_token_count = 0;

    frokenizer::Tokenizer tokenizer;

    LOG_INFO("running frokenizer on corpus ...");
    bool success = tokenizer.encode(
        corpus_data.data(), 
        corpus_data.size(), 
        actual_tokens.data(), 
        actual_token_count, 
        actual_tokens.size()
    );

    ASSERT(success, "frokenizer::encode returned false (failure during tokenization)");

    LOG_INFO("frokenizer output token count: %zu", actual_token_count);
    
    ASSERT(actual_token_count == expected_token_count, "token count mismatch between frokenizer and ground truth");
    
    LOG_INFO("token counts match. performing full stream diff ...");
    
    size_t mismatch_count = 0;
    const size_t max_errors_to_print = 100; // avoid terminal flooding

    for (size_t i = 0; i < expected_token_count; ++i) 
    {
        if (__builtin_expect(actual_tokens[i] != expected_tokens[i], 0)) 
        {
            mismatch_count++;
            if (mismatch_count <= max_errors_to_print) 
            {
                printf(C_RED "Mismatch #%zu at token index %zu" C_RESET "\n", mismatch_count, i);
                printf("    expected token id: %-6u  actual token id: %-6u\n", expected_tokens[i], actual_tokens[i]);

                char exp_str_buf[256] = {0};
                char act_str_buf[256] = {0};
                size_t exp_len = 0, act_len = 0;
                
                tokenizer.decode(expected_tokens[i], exp_str_buf, exp_len, 255);
                tokenizer.decode(actual_tokens[i], act_str_buf, act_len, 255);

                printf("    expected decodes to: '%.*s'\n", (int)exp_len, exp_str_buf);
                printf("    actual   decodes to: '%.*s'\n", (int)act_len, act_str_buf);
                printf("\n");
            }
        }
    }

    ASSERT(mismatch_count == 0, "found bitwise mismatches in token stream between frokenizer and tiktoken");
}

int main(int argc, char** argv) 
{
    if (argc != 3) 
    {
        LOG_ERROR("usage: test_parity <corpus.txt> <expected.bin>");
        return 1;
    }

    printf(C_YELLOW "[*]" C_RESET " running parity check ...\n");
    run_parity_test(argv[1], argv[2]);
    printf(C_GREEN "[PASS]" C_RESET " parity check \n");

    printf("\n");
    printf(C_GREEN "[SUCCESS]" C_RESET " ============================================= \n");
    printf(C_GREEN "[SUCCESS]" C_RESET "  OUTPUT IS BYTE-FOR-BYTE IDENTICAL TO PYTHON  \n");
    printf(C_GREEN "[SUCCESS]" C_RESET " ============================================= \n");

    return 0;
}
