// lexer.re.hpp
#ifndef LEXER_HPP
#define LEXER_HPP

#include <cstddef>
#include <cstdint>

/*
I got the pretokenize regex from here:
https://huggingface.co/Qwen/Qwen3.5-9B/raw/main/tokenizer_config.json

"pretokenize_regex": 
"(?i:'s|'t|'re|'ve|'m|'ll|'d)|[^\\r\\n\\p{L}\\p{N}]?[\\p{L}\\p{M}]+|\\p{N}| ?[^\\s\\p{L}\\p{M}\\p{N}]+[\\r\\n]*|\\s*[\\r\\n]+|\\s+(?!\\S)|\\s+"
*/

namespace frokenizer 
{

enum TokenType : int 
{
    TOKEN_EOF = 0,
    TOKEN_ERROR = -1,
    TOKEN_CHUNK = 1
};

struct Token 
{
    const char* start;
    size_t length;
    TokenType type;
};

struct LexerState 
{
    const char* cursor;
    const char* limit;
    const char* marker;
};

inline void lexer_init(LexerState* state, const char* buffer, size_t buffer_length) 
{
    state->cursor = buffer;
    state->limit = buffer + buffer_length;
    state->marker = nullptr;
}

/*!include:re2c "unicode_categories.re" */

[[nodiscard]] inline TokenType lexer_next(LexerState* state, Token* out_token) 
{
    if (state->cursor >= state->limit) 
    {
        out_token->start = state->cursor;
        out_token->length = 0;
        out_token->type = TOKEN_EOF;
        return TOKEN_EOF;
    }

    const char* YYCURSOR = state->cursor;
    const char* YYLIMIT = state->limit;
    const char* YYMARKER = state->marker;
    const char* token_start = YYCURSOR;
    
    #define EMIT(len) do { \
        out_token->start = token_start; \
        out_token->length = (len); \
        out_token->type = TOKEN_CHUNK; \
        state->cursor = token_start + (len); \
        state->marker = YYMARKER; \
        return TOKEN_CHUNK; \
    } while(0)

    /*!re2c
        re2c:api = custom;
        re2c:api:style = free-form;
        re2c:define:YYCTYPE = "unsigned char";
        
        // virtual null-terminator and bounds-checking primitives
        re2c:define:YYPEEK     = "YYCURSOR < YYLIMIT ? *YYCURSOR : 0";
        re2c:define:YYSKIP     = "++YYCURSOR;";
        re2c:define:YYBACKUP   = "YYMARKER = YYCURSOR;";
        re2c:define:YYRESTORE  = "YYCURSOR = YYMARKER;";
        re2c:define:YYLESSTHAN = "YYCURSOR >= YYLIMIT";
        re2c:define:YYEND      = "YYCURSOR >= YYLIMIT";
        
        re2c:yyfill:enable = 0;
        re2c:eof = 0;
        re2c:encoding:utf8 = 1;

        cat_L = Lu | Ll | Lt | Lm | Lo;
        cat_M = Mn | Mc | Me;
        cat_N = Nd | Nl | No;
        cat_Z = Zs | Zl | Zp;

        s_not_rn    = [ \t\v\f\x85] | cat_Z;
        s           = s_not_rn | [\r\n];
        not_rn_L_N  = [^] \ [\r\n] \ cat_L \ cat_N;
        not_s_L_M_N = [^] \ s \ cat_L \ cat_M \ cat_N;
        L_M         = cat_L | cat_M;

        // --------------------------------------------------------------------
        // rules
        // --------------------------------------------------------------------

        "'" [sStTmMdD] L_M+ { EMIT(2); }
        "'" ([rR][eE] | [vV][eE] | [lL][lL]) L_M+ { EMIT(3); }
        "'" [sStTmMdD] { EMIT(YYCURSOR - token_start); }
        "'" ([rR][eE] | [vV][eE] | [lL][lL]) { EMIT(YYCURSOR - token_start); }

        not_rn_L_N? L_M+ { EMIT(YYCURSOR - token_start); }

        cat_N { EMIT(YYCURSOR - token_start); }

        " "? not_s_L_M_N+ [\r\n]* { EMIT(YYCURSOR - token_start); }

        s* [\r\n]+ { EMIT(YYCURSOR - token_start); }

        s_not_rn+ {
            size_t len = YYCURSOR - token_start;
            // Emulate Python's \s+(?!\S) backtracking
            if (YYCURSOR < YYLIMIT && len > 1) {
                len -= 1;
            }
            EMIT(len);
        }

        // --------------------------------------------------------------------
        // fallbacks
        // --------------------------------------------------------------------
        * {
            out_token->start = token_start;
            out_token->length = YYCURSOR - token_start;
            out_token->type = TOKEN_ERROR;
            state->cursor = YYCURSOR;
            state->marker = YYMARKER;
            return TOKEN_ERROR;
        }

        $ {
            out_token->start = token_start;
            out_token->length = 0;
            out_token->type = TOKEN_EOF;
            state->cursor = YYCURSOR;
            state->marker = YYMARKER;
            return TOKEN_EOF;
        }
    */

    #undef EMIT
}

} // namespace frokenizer
#endif // LEXER_HPP
