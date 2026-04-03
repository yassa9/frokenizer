// bench_single.cpp
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <chrono>

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
    if (argc != 2) 
    {
        fprintf(stderr, "usage: %s <corpus_path>\n", argv[0]);
        return 1;
    }

    std::vector<char> corpus = read_file(argv[1]);
    double mb_size = corpus.size() / (1024.0 * 1024.0);

    frokenizer::Tokenizer tokenizer;
    
    // allocate enough space for worst-case tokenization
    std::vector<uint32_t> out(corpus.size() + 1024);
    size_t out_len = 0;

    // warmup cache
    tokenizer.encode(corpus.data(), 1024, out.data(), out_len, out.size());
    out_len = 0;

    auto start = std::chrono::high_resolution_clock::now();
    tokenizer.encode(corpus.data(), corpus.size(), out.data(), out_len, out.size());
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    
    double throughput = mb_size / diff.count();
    
    // print only the throughput so python can read it
    printf("%.2f\n", throughput);

    return 0;
}
