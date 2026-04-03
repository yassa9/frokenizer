# gen_gt.py
import tiktoken
import tiktoken.load
import struct
import sys

def generate_truth(corpus_path, vocab_path, output_bin_path):
    print(f"[*] loading custom qwen vocabulary from {vocab_path} ...")
    
    try:
        mergeable_ranks = tiktoken.load.load_tiktoken_bpe(vocab_path)
    except Exception as e:
        print(f"[-] failed to load {vocab_path}: {e}")
        sys.exit(1)

    qwen_regex = r"(?i:'s|'t|'re|'ve|'m|'ll|'d)|[^\r\n\p{L}\p{N}]?[\p{L}\p{M}]+|\p{N}| ?[^\s\p{L}\p{M}\p{N}]+[\r\n]*|\s*[\r\n]+|\s+(?!\S)|\s+"

    special_tokens = {
        "<|endoftext|>": 151643,
        "<|im_start|>":  151644,
        "<|im_end|>":    151645
    }

    enc = tiktoken.Encoding(
        name="qwen_custom",
        pat_str=qwen_regex,
        mergeable_ranks=mergeable_ranks,
        special_tokens=special_tokens
    )
    
    print(f"[*] reading {corpus_path} ...")
    with open(corpus_path, "r", encoding="utf-8", newline="") as f:
        text = f.read()

    print(f"[*] tokenizing {len(text)} bytes of text ...")
    
    tokens = enc.encode_ordinary(text)

    print(f"[*] writing {len(tokens)} tokens to {output_bin_path} ...")
    with open(output_bin_path, "wb") as f:
        # write as little-endian unsigned 32-bit integers
        f.write(struct.pack(f"<{len(tokens)}I", *tokens))
        
    print("[*] ground truth generation complete.")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("usage: python3 gen_gt.py <corpus.txt> <qwen.tiktoken> <expected.bin>")
        sys.exit(1)
    generate_truth(sys.argv[1], sys.argv[2], sys.argv[3])
