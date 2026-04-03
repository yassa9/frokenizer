# gen_stubborn.py
import os
import sys

def generate_torture_corpus(output_path, target_size_mb):
    print(f"[*] generating {target_size_mb}MB torture corpus at {output_path} ...")
    
    blocks = [
        # 1. contractions (case insensitive)
        "I'm testing this. You're going to pass. He'll do it. THEY'VE done it! wouldn't shouldn't let's.",
        
        # 2. unicode letters (\p{L}) and marks (\p{M})
        "Français, Español, München, naïve, coöperate.", # accents
        "مَرْحَبًا بِكُمْ فِي هَذَا الِاخْتِبَارِ.",                    # arabic with marks (harakat)
        "这是中文测试。我们来看看分词器怎么处理它。",    # chinese (no spaces)
        "Русский язык очень интересный.",                # cyrillic
        "निबंध (Hindi), ำ (Thai mark).", #
        
        # 3. numbers (\p{N})
        "There are 123 apples, 456.789 dollars, and 10,000,000 users. IPv4: 192.168.0.1",
        
        # 4. symbol clusters and punctuation
        "!!! ??? ,,, ... --- ===> <= >= != :: && ||",
        
        # 5. code snippets
        "int main() {\n    std::vector<int> v = {1, 2, 3};\n    if (v.size() > 0) return 1;\n}",
        "def foo(x):\n\treturn x ** 2 + (y // 3)\n\n\n",
        
        # 6. spacing torture (\s+, \s*[\r\n]+, \s+(?!\S))
        "Word    Word\tWord\vWord\fWord",
        "Trailing spaces test:    ",
        "Carriage return test\r\nAnother line\r\n\r\nDouble line",
        "Spaces before newline  \n   Spaces after newline",
        
        # 7. emojis and 4-byte UTF-8
        "Hello 🌍! 👨‍👩‍👧‍👦 🚀🔥 💻",
        
        # 8. edge cases (single chars, alternating)
        "a 1 b 2 c 3 d 4 e 5",
        "a,b,c,d,e,f,g",
        "  \n  \n  \n",
    ]

    base_text = "\n\n--- BLOCK BOUNDARY ---\n\n".join(blocks)

    base_size = len(base_text.encode('utf-8'))
    multiplier = (target_size_mb * 1024 * 1024) // base_size
    
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    
    with open(output_path, "w", encoding="utf-8", newline="") as f:
        for _ in range(multiplier): 
            f.write(base_text)
            f.write("\n")
            
    print(f"[+] successfully generated the corpus.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"usage:   python3 gen_stubborn.py <output_path> <target_size_mb>")
        print(f"example: python3 gen_stubborn.py torture.txt 16")
        sys.exit(1)

    output_path = sys.argv[1]
    
    # default is 16, otherwise take 2nd argument
    target_size_mb = 16
    if len(sys.argv) >= 3:
        try:
            target_size_mb = int(sys.argv[2])
        except ValueError:
            print(f"error: target_size_mb must be an integer (you provided: '{sys.argv[2]}')")
            sys.exit(1)

    print(f"[*] generating {target_size_mb}MB corpus to {output_path} ...")
    generate_torture_corpus(output_path, target_size_mb=target_size_mb)

