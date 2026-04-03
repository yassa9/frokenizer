// frokenizer.hpp
// Copyright (c) 2026 Yassa Sfen
// SPDX-License-Identifier: MIT

#ifndef FROKENIZER_HPP
#define FROKENIZER_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "frokenizer_generated/lexer.hpp"
#include "frokenizer_generated/baked.hpp"

namespace frokenizer 
{

struct special_token_def 
{
    const char* str;
    size_t len;
    uint32_t id;
};

static constexpr special_token_def special_tokens[] = 
{
    {"<|endoftext|>", 13, 151643},
    {"<|im_start|>",  12, 151644},
    {"<|im_end|>",    10, 151645}
};
static constexpr size_t num_special_tokens = sizeof(special_tokens) / sizeof(special_tokens[0]);

// returns pointer to the earliest special token found, or nullptr
inline const char* find_earliest_special_token(const char* text, size_t len, uint32_t& out_id, size_t& out_len) 
{
    if (__builtin_expect(len == 0, 0)) return nullptr;

    const char* p = text;
    const char* end_search = text + len;

    while ((p = static_cast<const char*>(memchr(p, '<', end_search - p))) != nullptr) 
    {
        size_t remaining = end_search - p;

        for (size_t i = 0; i < num_special_tokens; ++i) 
        {
            const special_token_def& st = special_tokens[i];
            if (remaining < st.len) continue;

            if (memcmp(p, st.str, st.len) == 0) 
            {
                out_id = st.id;
                out_len = st.len;
                return p;
            }
        }
        
        p++;
    }

    return nullptr;
}

class Tokenizer 
{
    static constexpr size_t max_chunk = 4096;
    static constexpr uint64_t magic_64 = 11400714819323198485ull;
    static constexpr size_t max_small_chunk = 64;

    // null pointer -> 0xFFFF
    struct TokenNode 
    {
        uint32_t id;
        uint16_t prev;
        uint16_t next;
        uint32_t rank;
    };

    struct HeapNode 
    {
        uint32_t rank;
        uint16_t pos;
        [[nodiscard]] inline uint64_t score() const 
        {
            return (static_cast<uint64_t>(rank) << 32) | pos;
        }
    };

    TokenNode nodes[max_chunk];
    HeapNode   heap[max_chunk];
    uint16_t  heap_sz = 0;

    inline void heap_push(uint32_t rank, uint16_t pos) 
    {
        uint16_t i = heap_sz++;
        uint64_t s = (static_cast<uint64_t>(rank) << 32) | pos;
        while (__builtin_expect(i > 0, 1)) 
        {
            uint16_t p = (i - 1) / 2;
            if (__builtin_expect(heap[p].score() <= s, 1)) break;
            heap[i] = heap[p];
            i = p;
        }
        heap[i] = {rank, pos};
    }

    inline HeapNode heap_pop() 
    {
        HeapNode root = heap[0];
        HeapNode last = heap[--heap_sz];
        uint64_t last_s = last.score();
        uint16_t i = 0;
        while (true) 
        {
            uint16_t  left = i * 2 + 1;
            uint16_t right = i * 2 + 2;
            if (__builtin_expect(left >= heap_sz, 0)) break;
            uint16_t min_child = (right < heap_sz && heap[right].score() < heap[left].score()) ? right : left;
            if (__builtin_expect(last_s <= heap[min_child].score(), 1)) break;
            heap[i] = heap[min_child];
            i = min_child;
        }
        heap[i] = last;
        return root;
    }

    inline uint32_t get_merge(uint32_t left, uint32_t right) 
    {
        uint64_t key = ((uint64_t)left << 32) | right;
        uint32_t mask = hash_table_size - 1;
        uint32_t idx = (key * magic_64) >> 46; 
        
        if (__builtin_expect(merge_table[idx].key == key, 1)) return merge_table[idx].result;
        if (__builtin_expect(merge_table[idx].key == empty_key, 0)) return ~0u;
        
        while (true) 
        {
            idx = (idx + 1) & mask;
            if (__builtin_expect(merge_table[idx].key == key, 1)) return merge_table[idx].result;
            if (__builtin_expect(merge_table[idx].key == empty_key, 0)) return ~0u;
        }
    }

    inline uint64_t fnv1a(const char* data, size_t len) 
    {
        uint64_t h = 14695981039346656037ull;
        for (size_t i = 0; i < len; ++i) 
        {
            h ^= static_cast<unsigned char>(data[i]);
            h *= 1099511628211ull;
        }
        return h;
    }

    __attribute__((hot))
    inline void encode_chunk(const char* text, size_t len, uint32_t* out, size_t& out_len, size_t max_out) 
    {
        if (__builtin_expect(len == 1, 0)) 
        {
            out[out_len++] = base_to_rank[static_cast<unsigned char>(text[0])];
            return;
        }
        if (__builtin_expect(len == 2, 0)) 
        {
            uint32_t r = base_pair_ranks[static_cast<unsigned char>(text[0])][static_cast<unsigned char>(text[1])];
            if (r != ~0u) { out[out_len++] = r; } 
            else 
            { 
                out[out_len++] = base_to_rank[static_cast<unsigned char>(text[0])]; 
                out[out_len++] = base_to_rank[static_cast<unsigned char>(text[1])]; 
            }
            return;
        }

        heap_sz = 0;
        nodes[0].id = base_to_rank[static_cast<unsigned char>(text[0])];
        nodes[0].prev = 0xffff;
        nodes[0].next = 1;
        nodes[0].rank = ~0u;

        for (size_t i = 1; i < len; ++i) 
        {
            unsigned char c = static_cast<unsigned char>(text[i]);
            nodes[i].id = base_to_rank[c];
            nodes[i].prev = static_cast<uint16_t>(i - 1);
            nodes[i].next = i + 1 < len ? static_cast<uint16_t>(i + 1) : 0xffff;
            nodes[i].rank = ~0u;

            uint32_t rank = base_pair_ranks[static_cast<unsigned char>(text[i-1])][c];
            nodes[i-1].rank = rank;
            if (rank != ~0u) heap_push(rank, static_cast<uint16_t>(i - 1));
        }

        while (__builtin_expect(heap_sz > 0, 1)) 
        {
            HeapNode top = heap_pop();
            uint16_t left = top.pos;
            
            // lazy deletion check: if the rank mutated, this tie is invalid
            if (__builtin_expect(nodes[left].rank != top.rank, 0)) continue;

            uint16_t right = nodes[left].next;
            nodes[left].id = top.rank; 
            uint16_t next = nodes[right].next;
            nodes[left].next = next;
            
            if (__builtin_expect(next != 0xffff, 1)) nodes[next].prev = left;
            
            nodes[left].rank = ~0u;
            nodes[right].rank = ~0u;

            if (__builtin_expect(nodes[left].prev != 0xffff, 1)) 
            {
                uint16_t prev = nodes[left].prev;
                uint32_t r = get_merge(nodes[prev].id, nodes[left].id);
                nodes[prev].rank = r;
                if (r != ~0u) heap_push(r, prev);
            }
            if (__builtin_expect(next != 0xffff, 1)) 
            {
                uint32_t r = get_merge(nodes[left].id, nodes[next].id);
                nodes[left].rank = r;
                if (r != ~0u) heap_push(r, left);
            }
        }

        uint16_t curr = 0;
        while (__builtin_expect(curr != 0xffff && out_len < max_out, 1)) 
        {
            out[out_len++] = nodes[curr].id;
            curr = nodes[curr].next;
        }
    }

    // encodes a safe substring free of special tokens using the lexer pipeline
    inline bool encode_safe_string(const char* text, size_t len, uint32_t* out, size_t& out_len, size_t max_out) 
    {
        if (len == 0) return true;

        LexerState state;
        lexer_init(&state, text, len);
        Token tok;

        uint32_t mask = hash_table_size - 1;

        while (__builtin_expect(lexer_next(&state, &tok) != TOKEN_EOF, 1)) 
        {
            if (__builtin_expect(out_len >= max_out, 0)) return false;
            
            if (__builtin_expect(tok.type == TOKEN_ERROR, 0)) 
            {
                for (size_t i = 0; i < tok.length && out_len < max_out; ++i) 
                    out[out_len++] = base_to_rank[static_cast<unsigned char>(tok.start[i])];
                continue;
            }

            const char* ptr = tok.start;
            size_t rem = tok.length;
            
            while (__builtin_expect(rem > 0, 1)) 
            {
                size_t w = rem > max_chunk ? max_chunk : rem;
                
                // safety rollback: never split in the middle of a utf-8 sequence
                if (__builtin_expect(w == max_chunk && w < rem, 0)) 
                {
                    while (w > 0 && (ptr[w] & 0xc0) == 0x80) { w--; }
                    
                    // fallback if corrupted
                    if (__builtin_expect(w == 0, 0)) w = max_chunk; 
                }
                
                uint64_t h = fnv1a(ptr, w);
                uint32_t idx = h & mask;
                bool fast_found = false;
                
                while (true) 
                {
                    uint64_t tbl_h = chunk_table[idx].hash;
                    if (__builtin_expect(tbl_h == empty_key, 0)) break;
                    if (__builtin_expect(tbl_h == h, 1)) 
                    {
                        uint32_t tid = chunk_table[idx].token_id;
                        if (__builtin_expect(vocab_lengths[tid] == w && memcmp(&vocab_string_data[vocab_offsets[tid]], ptr, w) == 0, 1)) 
                        {
                            out[out_len++] = tid;
                            fast_found = true;
                            break;
                        }
                    }
                    idx = (idx + 1) & mask;
                }

                if (__builtin_expect(!fast_found, 0)) 
                {
                    if (__builtin_expect(w <= max_small_chunk, 1)) 
                    {
                        encode_small_chunk(ptr, w, out, out_len, max_out);
                    } else 
                    {
                        encode_chunk(ptr, w, out, out_len, max_out); 
                    }
                }

                ptr += w; rem -= w;
            }
        }
        return true;
    }

    struct small_node 
    {
        uint32_t id;
        uint32_t rank;
    };

    __attribute__((hot))
    inline void encode_small_chunk(const char* text, size_t len, uint32_t* out, size_t& out_len, size_t max_out) 
    {
        small_node parts[max_small_chunk];

        for (size_t i = 0; i < len; ++i) 
        {
            parts[i].id = base_to_rank[static_cast<unsigned char>(text[i])];
        }

        for (size_t i = 0; i < len - 1; ++i) 
        {
            parts[i].rank = base_pair_ranks[static_cast<unsigned char>(text[i])][static_cast<unsigned char>(text[i+1])];
        }
        parts[len - 1].rank = ~0u;

        size_t active_len = len;

        while (__builtin_expect(active_len > 1, 1)) 
        {
            uint32_t min_rank = ~0u;
            size_t min_idx = 0;
            
            for (size_t i = 0; i < active_len - 1; ++i) 
            {
                if (parts[i].rank < min_rank) 
                {
                    min_rank = parts[i].rank;
                    min_idx = i;
                }
            }

            if (__builtin_expect(min_rank == ~0u, 0)) break;

            // merge parts[min_idx] and parts[min_idx + 1]
            parts[min_idx].id = min_rank;

            // shift the rest of the array left by 1
            size_t elements_to_move = active_len - min_idx - 2;
            if (__builtin_expect(elements_to_move > 0, 1)) 
            {
                __builtin_memmove(&parts[min_idx + 1], &parts[min_idx + 2], elements_to_move * sizeof(small_node));
            }
            active_len--;

            if (__builtin_expect(min_idx + 1 < active_len, 1)) 
            {
                parts[min_idx].rank = get_merge(parts[min_idx].id, parts[min_idx + 1].id);
            } else {
                parts[min_idx].rank = ~0u;
            }

            if (__builtin_expect(min_idx > 0, 1)) 
            {
                parts[min_idx - 1].rank = get_merge(parts[min_idx - 1].id, parts[min_idx].id);
            }
        }

        for (size_t i = 0; i < active_len; ++i) 
        {
            if (__builtin_expect(out_len < max_out, 1)) 
            {
                out[out_len++] = parts[i].id;
            }
        }
    }

public:
    Tokenizer() = default;

    bool encode(const char* text, size_t len, uint32_t* out, size_t& out_len, size_t max_out) 
    {
        out_len = 0;
        const char* p = text;
        size_t rem = len;

        // main loop: 
        // - find special tokens 
        // - encode text before it
        // - append the special token
        while (rem > 0) 
        {
            uint32_t st_id = 0;
            size_t st_len = 0;
            const char* st_ptr = find_earliest_special_token(p, rem, st_id, st_len);

            if (st_ptr) 
            {
                size_t prefix_len = st_ptr - p;
                if (prefix_len > 0) 
                {
                    if (__builtin_expect(!encode_safe_string(p, prefix_len, out, out_len, max_out), 0)) return false;
                }
                if (__builtin_expect(out_len < max_out, 1)) 
                {
                    out[out_len++] = st_id;
                } else return false;
                
                size_t consumed = prefix_len + st_len;
                p += consumed;
                rem -= consumed;
            } 
            else 
            {
                return encode_safe_string(p, rem, out, out_len, max_out);
            }
        }
        return true;
    }

    inline bool decode(uint32_t token_id, char* out_str, size_t& out_len, size_t max_len) 
    {
        // check if it is a hardcoded special token first
        for (size_t i = 0; i < num_special_tokens; ++i) 
        {
            if (__builtin_expect(special_tokens[i].id == token_id, 0)) 
            {
                size_t t_len = special_tokens[i].len;
                if (__builtin_expect(out_len + t_len > max_len, 0)) return false;
                memcpy(out_str + out_len, special_tokens[i].str, t_len);
                out_len += t_len;
                return true;
            }
        }

        // otherwise, check the baked bpe vocabulary
        if (__builtin_expect(token_id >= vocab_array_size, 0)) return false;
        
        size_t t_len = vocab_lengths[token_id];
        if (__builtin_expect(out_len + t_len > max_len, 0)) return false;
        
        memcpy(out_str + out_len, &vocab_string_data[vocab_offsets[token_id]], t_len);
        out_len += t_len;
        return true;
    }
};

} // namespace frokenizer

// FROKENIZER_HPP
#endif 
