/*
 *
 */

#ifndef _lpp_lex_h
#define _lpp_lex_h

#include "iro/common.h"
#include "iro/containers/array.h"
#include "iro/logger.h"

#include "source.h"

#include "csetjmp"

namespace lpp
{

struct Lpp;

/* ============================================================================
 */
struct Token
{
  enum class Kind
  {
    Invalid,

    Eof,

    LuaLine,
    LuaInline,
    LuaBlock,

    MacroSymbol, // @
    MacroSymbolImmediate, // @@
    MacroIdentifier,
    MacroMethod, // a macro that uses : to call a method
    MacroTupleArg, // (a, b, c, ...)
    MacroStringArg,   // "..."
    MacroHereDocArg, // <-[term] ... ([term]|->)

    // Any text that lpp is preprocessing. Eg. if we 
    // are preprocessing a C file, this is C code.
    Document, 

    DollarSign,

    Whitespace,
  };

  Kind kind = Kind::Invalid;

  s32 loc = 0;
  s32 len = 0;

  s32 macro_indent_loc = 0;
  s32 macro_indent_len = 0;

  s32 method_colon_offset = 0;

  // Retrieve the raw string this token encompasses from the given Source
  String getRaw(Source* src) 
  { 
    if (kind == Kind::MacroHereDocArg)
      return src->getVirtualStr(loc, len);
    else
      return src->getStr(loc, len); 
  }
};

}

DefineNilValue(lpp::Token, {}, 
    { return x.kind == lpp::Token::Kind::Invalid; });

namespace lpp
{

/* ============================================================================
 *
 *  TODO(sushi) try reworking the lexer to give a token
 *              stream again instead of caching them into an array
 *              to save on memory. Making it a stream complicates the 
 *              internal logic too much at the moment so I'm just 
 *              going to avoid it for now.
 *              It's not a huge issue, though, because Tokens are quite small, 
 *              and with how lpp works we shouldn't be making too many tokens 
 *              to begin with.
 */
struct Lexer
{
  using enum Token::Kind;
  typedef Array<Token> TokenArray;

  utf8::Codepoint current_codepoint;
  u64 current_offset;

  Source* source;
  io::IO* in;

  TokenArray tokens;

  b8 in_indentation;
  b8 at_end;

  jmp_buf err_handler; // this is 200 bytes !!!

  b8   init(io::IO* input_stream, Source* src);
  void deinit();
        
  b8 run();


private:

  Token curt;

  b8 lexDocument();

  b8 lexLuaLine(); // '$'
  b8 lexLuaInline(); // '$'['<'...'>']'('...')'
  b8 lexLuaBlock(); // '$$$'
  b8 lexLuaLineOrInlineOrBlock();

  b8 lexMacro(); // '@'
  b8 lexMacroName();
  b8 lexMacroTupleArgs();
  b8 lexMacroStringArg();

  b8 readStreamIfNeeded(b8 peek);
  b8 decodeCurrent();

  void initCurt() { initCurtAt(current_offset); }
  void initCurtAt(u64 offset);
  void finishCurt(Token::Kind kind, s32 len_offset = 0);

  u32 current();
  u32 peek();

  b8 at(u8 c);
  b8 eof();
  b8 atFirstIdentifierChar();
  b8 atIdentifierChar();
  b8 atDigit();
  b8 atWhitespace();

  // Don't forget that this could move!
  Token* lookback(u64 n)
  {
    assert(n && n <= tokens.len());
    return &tokens.end()[-n];
  }

  b8 lookbackIs(u64 n, Token::Kind k)
  {
    return lookback(n)->kind == k;
  }

  b8 lookbackIs(Token::Kind k)
  {
    return lookbackIs(1, k);
  }

  void advance(s32 n = 1);
  void skipWhitespace(b8 supress_token = false);

  template<typename... T>
  b8 errorAt(s32 line, s32 column, T... args);

  template<typename... T>
  b8 errorAtToken(Token& t, T... args);

  template<typename... T>
  b8 errorHere(T... args);

  struct ScopedLexStage
  {
    Lexer& lex;
    ScopedLexStage(
        Lexer& lex,
        const char* funcname) 
      : lex(lex)
    {
      onEnter(funcname);
      lex.stage_depth += 1;
    }

    ~ScopedLexStage()
    {
      lex.stage_depth -= 1;
      onExit();
    }

    void onEnter(const char* funcname);
    void onExit();

    template<typename... T>
    void log(T... args);
  };

  u64 stage_depth = 0;
};

}

namespace iro::io
{

static s64 format(io::IO* io, lpp::Token::Kind& kind)
{
  switch (kind)
  {
#define c(x) \
    case lpp::Token::Kind::x: return formatv(io, color::magenta(STRINGIZE(x)));
  c(Invalid);
  c(Eof);
  c(LuaLine);
  c(LuaInline);
  c(LuaBlock);
  c(MacroSymbol);
  c(MacroSymbolImmediate);
  c(MacroIdentifier);
  c(MacroMethod);
  c(MacroTupleArg);
  c(MacroStringArg);
  c(MacroHereDocArg);
  c(Document);
  c(DollarSign);
  c(Whitespace);
#undef c
    default: assert(!"invalid token kind"); return 0;
  }
}

static s64 format(io::IO* io, lpp::Token& token)
{
  return io::formatv(io, 
      color::cyan("Token"), 
      '(', token.kind, ',', 
         color::green, 
         token.loc, '+', token.len, 
         color::reset, 
      ')');
}

}

#endif // _lpp_lex_h
