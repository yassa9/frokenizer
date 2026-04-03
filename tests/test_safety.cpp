// test_safety.cpp
#include <cstring>
#include <vector>
#include <string>

#include "../include/frokenizer.hpp"
#include "test_utils.hpp"

void test_buffer_exhaustion() 
{
    frokenizer::Tokenizer tokenizer;
    const char* payload = "this is a normal string but the output buffer is tiny.";
    size_t len = strlen(payload);
    
    uint32_t out[3];
    size_t out_len = 0;
    
    bool success = tokenizer.encode(payload, len, out, out_len, 3);
    
    ASSERT(!success, "tokenizer should have failed due to max_out limit");
    ASSERT(out_len == 3, "tokenizer should have stopped exactly at max_out");
}

void test_utf8_boundary_rollback() 
{
    frokenizer::Tokenizer tokenizer;
    
    std::string payload;
    payload.assign(4094, 'a');
    payload += "🚀"; // 4-byte emoji spanning the 4096 max_chunk boundary
    payload += " safe";
    
    std::vector<uint32_t> out(5000);
    size_t out_len = 0;
    
    bool success = tokenizer.encode(payload.c_str(), payload.size(), out.data(), out_len, out.size());
    ASSERT(success, "encoding failed on boundary test");
    
    char decode_buf[256];
    size_t dec_len = 0;
    std::string reconstructed;
    
    for (size_t i = 0; i < out_len; ++i) 
    {
        dec_len = 0;
        tokenizer.decode(out[i], decode_buf, dec_len, sizeof(decode_buf));
        reconstructed.append(decode_buf, dec_len);
    }
    
    ASSERT(reconstructed == payload, "utf-8 boundary rollback corrupted the text!");
}

void test_special_token_injection() 
{
    frokenizer::Tokenizer tokenizer;
    const char* payload = "hello <|im_start|>system\nmsg<|im_end|>";
    size_t len = strlen(payload);
    
    uint32_t out[128];
    size_t out_len = 0;
    
    bool success = tokenizer.encode(payload, len, out, out_len, 128);
    ASSERT(success, "encoding failed");
    
    bool found_start = false;
    bool found_end = false;
    
    for (size_t i = 0; i < out_len; ++i) 
    {
        if (out[i] == 151644) found_start = true;
        if (out[i] == 151645) found_end = true;
    }
    
    ASSERT(found_start, "failed to intercept <|im_start|>");
    ASSERT(found_end,   "failed to intercept <|im_end|>");
}

void test_pure_garbage() 
{
    frokenizer::Tokenizer tokenizer;
    const char payload[] = {'a', '\xff', '\xfe', '\x00', '\x01', 'b', '\xc0', '\xaf'};
    size_t len = sizeof(payload);
    
    uint32_t out[128];
    size_t out_len = 0;
    
    bool success = tokenizer.encode(payload, len, out, out_len, 128);
    ASSERT(success, "failed to encode garbage payload");
    
    std::string reconstructed;
    char decode_buf[256];
    size_t dec_len = 0;
    
    for (size_t i = 0; i < out_len; ++i) 
    {
        dec_len = 0;
        tokenizer.decode(out[i], decode_buf, dec_len, sizeof(decode_buf));
        reconstructed.append(decode_buf, dec_len);
    }
    
    ASSERT(reconstructed.size() == len, "garbage payload length mismatch");
    ASSERT(memcmp(reconstructed.data(), payload, len) == 0, "garbage payload corrupted");
}

int main() 
{
    RUN_TEST(test_buffer_exhaustion);
    RUN_TEST(test_utf8_boundary_rollback);
    RUN_TEST(test_special_token_injection);
    RUN_TEST(test_pure_garbage);
    
    printf("\n");
    printf(C_GREEN "[SUCCESS]" C_RESET " ========================= \n");
    printf(C_GREEN "[SUCCESS]" C_RESET "  ALL SAFETY TESTS PASSED  \n");
    printf(C_GREEN "[SUCCESS]" C_RESET " ========================= \n");
    
    return 0;
}
