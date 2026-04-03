# bench_multi.py
import os
import sys
import time
import subprocess
import tiktoken
import tiktoken.load

def run_frokenizer(binary_path, threads, corpus_path):
    cmd = [binary_path, str(threads), corpus_path]
    result = subprocess.run(cmd, capture_output=True, text=True)
    try:
        return float(result.stdout.strip())
    except:
        print(f"[-] C++ multi-core binary failed on {threads} threads")
        return 0.0

def run_tiktoken(threads, corpus_chunks, mb_size, enc):
    start = time.perf_counter()
    _ = enc.encode_ordinary_batch(corpus_chunks, num_threads=threads)
    end = time.perf_counter()
    return mb_size / (end - start)

def main():
    if len(sys.argv) != 4:
        print("usage: python3 bench_multi.py <binary_path> <corpus.txt> <qwen.tiktoken>")
        sys.exit(1)
        
    binary_path = sys.argv[1]
    corpus_path = sys.argv[2]
    vocab_path = sys.argv[3]
    
    print("[*] loading data and warming up engines ...")
    
    with open(corpus_path, "r", encoding="utf-8") as f:
        full_text = f.read()
    
    corpus_bytes = full_text.encode('utf-8')
    mb_size = len(corpus_bytes) / (1024 * 1024)
    
    num_chunks = 128
    c_len = len(full_text) // num_chunks
    corpus_chunks = [full_text[i:i+c_len] for i in range(0, len(full_text), c_len)]
    if len(full_text) % num_chunks != 0:
        corpus_chunks.append(full_text[num_chunks*c_len:])

    mergeable_ranks = tiktoken.load.load_tiktoken_bpe(vocab_path)
    qwen_regex = r"(?i:'s|'t|'re|'ve|'m|'ll|'d)|[^\r\n\p{L}\p{N}]?[\p{L}\p{M}]+|\p{N}| ?[^\s\p{L}\p{M}\p{N}]+[\r\n]*|\s*[\r\n]+|\s+(?!\S)|\s+"
    tik_enc = tiktoken.Encoding(
        name="qwen_exact", pat_str=qwen_regex, mergeable_ranks=mergeable_ranks,
        special_tokens={"<|endoftext|>": 151643, "<|im_start|>": 151644, "<|im_end|>": 151645}
    )
    
    max_threads = os.cpu_count() or 4
    
    # generate: 1, then multiples of 4, then max_threads
    thread_counts = [1]
    thread_counts.extend([t for t in range(4, max_threads, 4)])
    if max_threads not in thread_counts:
        thread_counts.append(max_threads)
    
    header_cols = [f"{t} THREAD{'S' if t > 1 else ''}" for t in thread_counts]
    header_str = f"{'ENGINE':<20} | " + " | ".join(f"{c:>10}" for c in header_cols)
    
    print("\n" + "=" * len(header_str))
    print(header_str)
    print("=" * len(header_str))

    print(f"{'Frokenizer   (MB/s)':<20}", end="", flush=True)
    for t in thread_counts:
        res = run_frokenizer(binary_path, t, corpus_path)
        print(f" | {res:>10.2f}", end="", flush=True)
    print()

    print(f"{'Tiktoken     (MB/s)':<20}", end="", flush=True)
    for t in thread_counts:
        res = run_tiktoken(t, corpus_chunks, mb_size, tik_enc)
        print(f" | {res:>10.2f}", end="", flush=True)
    print()

    print("=" * len(header_str) + "\n")

if __name__ == "__main__":
    main()
