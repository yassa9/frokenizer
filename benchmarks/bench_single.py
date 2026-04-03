# bench_single.py
import os
import sys
import time
import subprocess
import tiktoken
import tiktoken.load

# force tiktoken to use strictly 1 thread
os.environ["RAYON_NUM_THREADS"] = "1"
os.environ["OMP_NUM_THREADS"] = "1"

def run_frokenizer(binary_path, corpus_path):
    print("[*] running frokenizer (strict single-core) ...")
    cmd = [binary_path, corpus_path]
    result = subprocess.run(cmd, capture_output=True, text=True)
    try:
        return float(result.stdout.strip())
    except ValueError:
        print("[-] c++ binary failed:", result.stderr)
        return 0.0

def run_tiktoken(corpus_path, vocab_path):
    print("[*] running tiktoken   (strict single-core) ...")
    mergeable_ranks = tiktoken.load.load_tiktoken_bpe(vocab_path)
    qwen_regex = r"(?i:'s|'t|'re|'ve|'m|'ll|'d)|[^\r\n\p{L}\p{N}]?[\p{L}\p{M}]+|\p{N}| ?[^\s\p{L}\p{M}\p{N}]+[\r\n]*|\s*[\r\n]+|\s+(?!\S)|\s+"
    
    enc = tiktoken.Encoding(
        name="qwen_exact",
        pat_str=qwen_regex,
        mergeable_ranks=mergeable_ranks,
        special_tokens={
            "<|endoftext|>": 151643, 
            "<|im_start|>": 151644, 
            "<|im_end|>": 151645
        }
    )
    
    with open(corpus_path, "r", encoding="utf-8") as f:
        text = f.read()
        
    mb_size = len(text.encode('utf-8')) / (1024 * 1024)
    
    _ = enc.encode_ordinary(text[:1024]) # warmup
    start = time.perf_counter()
    _ = enc.encode_ordinary(text)
    end = time.perf_counter()
    
    return mb_size / (end - start)

def main():
    if len(sys.argv) != 4:
        print("usage: python3 bench_single.py <binary_path> <corpus.txt> <qwen.tiktoken>")
        sys.exit(1)
        
    binary_path = sys.argv[1]
    corpus_path = sys.argv[2]
    vocab_path = sys.argv[3]
    
    frok_thru = run_frokenizer(binary_path, corpus_path)
    tikt_thru = run_tiktoken(corpus_path, vocab_path)

    print("\n")
    print(f"{'Tiktoken   (1 Thread)':<22} = {tikt_thru:>8.2f} MB/s")
    print(f"{'Frokenizer (1 Thread)':<22} = {frok_thru:>8.2f} MB/s")

if __name__ == "__main__":
    main()
