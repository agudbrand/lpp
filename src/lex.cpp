#include "lex.h"

#include "iro/logger.h"
#include "iro/platform.h"

#include "ctype.h"
#include "assert.h"

namespace lpp
{

static Logger logger = 
  Logger::create("lpp.lexer"_str, Logger::Verbosity::Notice);


/* ----------------------------------------------------------------------------
 */
void Lexer::ScopedLexStage::onEnter(const char* funcname)
{
  log("enter ", funcname, " at ", 
    io::SanitizeControlCharacters((char)lex.current()), "\n");
}

/* ----------------------------------------------------------------------------
 */
void Lexer::ScopedLexStage::onExit()
{
  log("leave at ", 
      io::SanitizeControlCharacters((char)lex.current()), "\n");
}

/* ----------------------------------------------------------------------------
 */
template<typename... T>
void Lexer::ScopedLexStage::log(T... args)
{
  for (u32 i = 0; i < lex.stage_depth; ++i)
    DEBUG(" ");
  DEBUG(args...);
}

#define LEX_STAGE \
  ScopedLexStage stage(*this, __func__)

/* ----------------------------------------------------------------------------
 */
u32 Lexer::current() { return current_codepoint.codepoint; }

b8 Lexer::at(u8 c) { return current() == c; }
b8 Lexer::eof()    { return at_end || at(0) || isnil(current_codepoint); }

static b8 isFirstIdentifierChar(u32 c) { return isalpha(c) || c == '_'; }
static b8 isIdentifierChar(u32 c) { return isFirstIdentifierChar(c) || 
                                           isdigit(c);  }

b8 Lexer::atFirstIdentifierChar() { return isFirstIdentifierChar(current()); }
b8 Lexer::atIdentifierChar() { return isIdentifierChar(current()); }

b8 Lexer::atWhitespace() { return 0 != isspace(current()); }

/* ----------------------------------------------------------------------------
 *  Returns false if we've reached the end of the buffer.
 */
b8 Lexer::readStreamIfNeeded(b8 peek)
{
  if (current_offset + (peek? current_codepoint.advance : 0) 
      >= source->cache.len)
  {
    TRACE("reading more bytes from stream... \n");

    Bytes reserved = source->cache.reserve(4096);
    s64 bytes_read = in->read(reserved);
    if (bytes_read == -1)
    {
      current_codepoint = nil;
      errorHere("failed to read more bytes from input stream");
      longjmp(err_handler, 0);
    }
    else if (bytes_read == 0)
    {
      TRACE("0 bytes read, must be the end of the file...\n");
      at_end = true;
      return false;
    }
    source->cache.commit(bytes_read);
    TRACE(bytes_read, " bytes read... chunk is: \n", 
      source->cache.asStr().sub(current_offset), "\n");
  }
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Lexer::decodeCurrent()
{
  current_codepoint = 
    utf8::decodeCharacter(
      source->cache.ptr + current_offset, 
      source->cache.len - current_offset);

  return notnil(current_codepoint);
}

/* ----------------------------------------------------------------------------
 */
u32 Lexer::peek()
{
  readStreamIfNeeded(true);
  u64 offset = current_offset + current_codepoint.advance;
  return utf8::decodeCharacter(
      source->cache.ptr + offset,
      source->cache.len - offset);
}

/* ----------------------------------------------------------------------------
 */
void Lexer::advance(s32 n)
{
  for (s32 i = 0; i < n; i++)
  {
    current_offset += current_codepoint.advance;

    if (!readStreamIfNeeded(false))
      return;

    if (!decodeCurrent())
    {
      errorHere("encountered invalid codepoint!");
      longjmp(err_handler, 0);
    }

    if (at('\n'))
    {
      in_indentation = true;
    }
    else if (in_indentation)
    {
      if (not isspace(current()))
        in_indentation = false;
    }
  }
}

/* ----------------------------------------------------------------------------
 */
template<typename... T>
b8 Lexer::errorAt(s32 line, s32 column, T... args)
{
  ERROR(source->name, ":", line, ":", column, ": ", args..., "\n");
  return false;
}

/* ----------------------------------------------------------------------------
 */
template<typename... T>
b8 Lexer::errorAtToken(Token& t, T... args)
{
  source->cacheLineOffsets();
  Source::Loc loc = source->getLoc(t.loc);
  return errorAt(loc.line, loc.column, args...);
}

/* ----------------------------------------------------------------------------
 */
template<typename... T>
b8 Lexer::errorHere(T... args)
{
  source->cacheLineOffsets();
  Source::Loc loc = source->getLoc(current_offset);
  return errorAt(loc.line, loc.column, args...);
}

/* ----------------------------------------------------------------------------
 */
void Lexer::skipWhitespace(b8 suppress_token)
{
  if (not atWhitespace())
    return;
  if (!suppress_token)
    initCurt();
  while (atWhitespace() and not eof())
    advance();
  if (!suppress_token)
    finishCurt(Token::Kind::Whitespace);
}

/* ----------------------------------------------------------------------------
 */
b8 Lexer::init(io::IO* input_stream, Source* src)
{
  assert(input_stream and src);

  TRACE("initializing with input stream '", src->name, "'\n");

  tokens = TokenArray::create();
  in = input_stream;
  source = src;
  at_end = false;
  current_offset = 0;
  current_codepoint = nil;

  // prep buffer and current
  readStreamIfNeeded(false);
  decodeCurrent();

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Lexer::deinit()
{
  tokens.destroy();
  *this = {};
}

/* ----------------------------------------------------------------------------
 */
b8 Lexer::run()
{
  LEX_STAGE;

  using enum Token::Kind;

  for (;;)
  {
    skipWhitespace();

    if (eof())
    {
      initCurt();
      finishCurt(Eof, 1);
      return true;
    }

    switch (current())
    {
    case '@':
      if (!lexMacro())
        return false;
      break;
    case '$':
      if (!lexLuaLineOrInlineOrBlock())
        return false;
      break;
    default:
      if (!lexDocument())
        return false;
      break;
    }
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Lexer::lexDocument()
{
  initCurt();

  u64 last_non_whitespace = current_offset;

  while (
      not at('@') and
      not at('$') and
      not eof())
  {
    if (at('\\'))
    {
      switch (peek())
      {
        case '$':
        case '@':
          // Escape an lpp symbol by splitting the document at the backslash.
          finishCurt(Document, -1);
          advance();
          initCurt();
      }
    }

    if (not atWhitespace())
      last_non_whitespace = current_offset;

    advance();
  }

  if (not eof() and last_non_whitespace < current_offset - 1)
  {
    u64 whitespace_start = last_non_whitespace + 1;
    finishCurt(Document, whitespace_start - current_offset);
    initCurtAt(whitespace_start);
    finishCurt(Whitespace);
  }
  else
    finishCurt(Document);

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Lexer::lexLuaLineOrInlineOrBlock()
{
  LEX_STAGE;

  assert(at('$'));
  initCurt();
  if (advance(), at('$'))
  {
    if (advance(), at('$'))
      return lexLuaBlock();
    else
      return errorHere("$$ has no meaning yet."_str);
  }
  else if (at('(') or at('<'))
    return lexLuaInline();
  else
    return lexLuaLine();
}

/* ----------------------------------------------------------------------------
 */
b8 Lexer::lexLuaBlock()
{
  LEX_STAGE;

  assert(at('$'));
  advance();
  initCurt();
  
  for (;;)
  {
    if ((advance(), at('$')) and
        (advance(), at('$')) and
        (advance(), at('$')))
    {
      advance();
      finishCurt(LuaBlock, -3);
      break;
    }

    if (eof())
      return errorAtToken(curt, 
        "unexpected eof while consuming lua block");
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Lexer::lexLuaInline()
{
  LEX_STAGE;

  assert(at('<') or at('('));
  if (at('('))
  {
    advance();
    initCurt();

    s64 nesting = 1;
    for (;;)
    {
      advance();
      if (eof()) 
        return errorAtToken(curt, 
            "unexpected eof while consuming inline lua expression");

      if (at('(')) 
        nesting += 1;
      else if (at(')'))
      {
        if (nesting == 1)
          break;
        else 
          nesting -= 1;
      }
    }

    finishCurt(LuaInline);
    advance();
  }
  else
    assert(!"need to implement inline attributes");
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Lexer::lexLuaLine()
{
  LEX_STAGE;

  initCurt();
  while (not at('\n') and not eof())
    advance();

  finishCurt(LuaLine);

  // Advance and init curt to prevent keeping the newline.
  advance();
  initCurt();

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Lexer::lexMacro()
{
  LEX_STAGE;

  initCurt();
  advance();
  Token::Kind macro_kind = MacroSymbol;
  if (at('@'))
  {
    advance();
    macro_kind = MacroSymbolImmediate;
  }
  finishCurt(macro_kind);

  skipWhitespace();

  if (!lexMacroName())
    return false;

  skipWhitespace();

  switch (current())
  {
  case '(':
    return lexMacroTupleArgs();
  case '"':
    return lexMacroStringArg();
  default:
    return true;
  }
}

/* ----------------------------------------------------------------------------
 */
b8 Lexer::lexMacroName()
{
  LEX_STAGE;

  initCurt();

  if (not atFirstIdentifierChar())
    return errorHere("expected an identifier of a macro after '@'");

  // NOTE(sushi) allow '.' so that we can index tables through macros, eg.
  //             $ local lib = {}
  //             $ lib.func = function() print("hi!") end
  //
  //             @lib.func()
  b8 found_colon = false;
  while (atIdentifierChar() or 
         at('.') or 
         at(':'))
  {
    if (at(':'))
    {
      // Peek ahead and see if an identifier character follows. If not 
      // then assume this is some document syntax being used directly
      // after a macro call, eg. a colon following a case expression.
      if (isFirstIdentifierChar(peek()))
        found_colon = true;
      break;
    }
    advance();
  }

  if (found_colon)
  {
    curt.method_colon_offset = current_offset - curt.loc;
    advance();
    while(atIdentifierChar())
      advance();

    if (at('.') or at(':'))
      return errorHere("cannot use ':' or '.' after method syntax");
  }

  if (found_colon)
    finishCurt(MacroMethod);
  else
    finishCurt(MacroIdentifier);

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Lexer::lexMacroTupleArgs()
{
  LEX_STAGE;

  assert(at('('));
  advance();
  skipWhitespace();

  if (at(')'))
  {
    // dont create a token for empty macro args
    advance();
    initCurt();
    return true;
  }

  u64 brace_nesting = 0;
  u64 paren_nesting = 1;

  initCurt();

  for (;;)
  {
    while (
        not at(',') and 
        not at(')') and 
        not at('{') and
        not at('}') and
        not at('(') and 
        not at(')') and
        not eof())
      advance();

    if (eof())
      return errorAtToken(curt, 
          "unexpected end of file while consuming macro arguments");

    b8 done = false;
    b8 reset_curt = false;
    switch (current())
    {
    case ',':
      // Tuple macro syntax allows using braces in an argument 
      // to suppress splitting the string by commas.
      if (brace_nesting == 0 &&
          paren_nesting == 1)
      {
        finishCurt(MacroTupleArg);
        reset_curt = true;
      }
      break;
    case '(':
      paren_nesting += 1;
      break;
    case ')':
      if (paren_nesting == 1)
        done = true;
      else
        paren_nesting -= 1;
      break;
    case '{':
      brace_nesting += 1;
      break;
    case '}':
      if (brace_nesting)
        brace_nesting -= 1;
      break;
    }

    if (done)
    {
      finishCurt(MacroTupleArg);
      break;
    }

    advance();
    skipWhitespace(true);
    if (reset_curt)
      initCurt();
  }

  if (lookbackIs(MacroTupleArg))
    advance(); // past the )

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Lexer::lexMacroStringArg()
{
  LEX_STAGE;

  advance();
  initCurt();

  for (;;)
  {
    advance();
    if (eof())
      return errorAtToken(curt, 
          "unexpected end of file while consuming macro string "
          "argument");

    if (at('"'))
      break;
  }
  finishCurt(MacroStringArg);
  advance();

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Lexer::initCurtAt(u64 offset)
{
  assert(offset <= current_offset);
  curt.kind = Token::Kind::Eof;
  curt.loc = offset;
}

/* ----------------------------------------------------------------------------
 */
void Lexer::finishCurt(Token::Kind kind, s32 len_offset)
{
  // assert(kind != Whitespace || !lookbackIs(Whitespace));
  s32 len = len_offset + current_offset - curt.loc;
  if (len > 0)
  {
    curt.kind = kind;
    curt.len = len;
    tokens.push(curt);

    DEBUG("finished ", curt, "\n");
    TRACE("==| ", io::SanitizeControlCharacters(curt.getRaw(source)), "\n");
  }
}

}

// TODO(sushi) heredoc syntax (usable outside of macro args as well)
      // Here-doc argument of syntax:
      //   <-
      //   ...
      //   ->
      // or, if a custom terminator is desired:
      //   <-TERM
      //   ...
      //   TERM
      // 
      // Everything inside of the terminators is considered a single string
      // with its indentation starting at the indentation of the final
      // terminator.
      // Eg.
      //    <-
      //      Hello
      //        There
      //      ->
      // Would result in the string:
      // Hello
      //   There
      //
      // TODO(sushi) implement the virtual source cache and then this.
      //
      //case '<':
      //  if (peek() != '-')
      //    break;

      //  advance(2);

      //  // Check if a custom terminator is provided.
      //  while (not at('\n') and atWhitespace())
      //    advance();

      //  {
      //    if (at('\n'))
      //    {
      //      while (not at('-') and not eof())
      //        advance();

      //      if (eof())
      //        return errorAt()
      //    }
      //    else
      //    {
      //      terminator.
      //    }
      //  }
