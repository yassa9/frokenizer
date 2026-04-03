// bench_multi.cpp
// Copyright (c) 2026 Yassa Sfen
// SPDX-License-Identifier: MIT

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <chrono>
#include <string_view>
#include <omp.h>

#include "../include/frokenizer.hpp"

std::vector<char> read_file(const char* path) 
{
    FILE* f = fopen(path, "rb");
    if (!f) exit(1);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> buf(len);
    fread(buf.data(), 1, len, f);
    fclose(f);
    return buf;
}

int main(int argc, char** argv) 
{
    if (argc != 3) 
    {
        fprintf(stderr, "usage: %s <threads> <corpus_path>\n", argv[0]);
        fprintf(stderr, "example: %s 8 data/corpus.txt\n", argv[0]);
        return 1;
    }

    int num_threads = std::atoi(argv[1]);
    const char* corpus_path = argv[2];

    std::vector<char> corpus = read_file(corpus_path);
    double mb_size = corpus.size() / (1024.0 * 1024.0);

    // split corpus by newlines to avoid cutting tokens across thread boundaries
    std::vector<std::string_view> chunks;
    const char* start_ptr = corpus.data();
    const char* end_ptr = corpus.data() + corpus.size();
    const char* p = start_ptr;

    while (p < end_ptr) 
    {
        const char* next = static_cast<const char*>(memchr(p, '\n', end_ptr - p));
        if (!next) next = end_ptr;
        else next++; 
        chunks.emplace_back(p, next - p);
        p = next;
    }

    omp_set_num_threads(num_threads);

    auto start = std::chrono::high_resolution_clock::now();

    #pragma omp parallel 
    {
        // each thread gets its own tokenizer (zero allocation overhead!)
        frokenizer::Tokenizer local_tokenizer;
        std::vector<uint32_t> local_out(1024 * 1024); // 1mb buffer per thread
        size_t local_out_len = 0;

        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < chunks.size(); ++i) 
        {
            local_out_len = 0;
            if (__builtin_expect(chunks[i].size() > local_out.size(), 0)) {
                local_out.resize(chunks[i].size() + 1024);
            }
            
            local_tokenizer.encode(
                chunks[i].data(), 
                chunks[i].size(), 
                local_out.data(), 
                local_out_len, 
                local_out.size()
            );
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    
    double throughput = mb_size / diff.count();
    printf("%.2f\n", throughput);

    return 0;
}
