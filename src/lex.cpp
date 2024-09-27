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
      == source->cache.len)
  {
    TRACE("reading more bytes from stream... \n");

    Bytes reserved = source->cache.reserve(128);
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
void Lexer::skipWhitespace()
{
  while (atWhitespace())
    advance();
}

/* ----------------------------------------------------------------------------
 */
b8 Lexer::init(io::IO* input_stream, Source* src)
{
  assert(input_stream and src);

  TRACE("initializing with input stream '", src->name, "'\n");
  SCOPED_INDENT;

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
  using enum Token::Kind;

  TRACE("run\n");

  for (;;)
  {
    initCurt();
    skipWhitespace();

    s32 trailing_space_start = 0;
    s32 trailing_space_len = 0;

    // TODO(sushi) implement escaping lpp's special characters by 
    //             splitting the document at the escape character.

    while (
        not at('@') and
        not at('$') and
        not eof())
    {
      if (trailing_space_len == 0)
      {
        if (isspace(current()) and not at('\n'))
        {
          trailing_space_start = current_offset;
          trailing_space_len = current_codepoint.advance;
        }
      }
      else
      {
        if (at('\n') or not isspace(current()))
          trailing_space_len = 0;
        else
          trailing_space_len += current_codepoint.advance;
      }

      advance();

    }

    finishCurt(Document);

    if (eof())
    {
      initCurt();
      finishCurt(Eof, 1);
      return true;
    }

    if (at('$'))
    {
      initCurt();
      advance();
      finishCurt(DollarSign);
      if (at('$'))
      {
        advance();
        if (at('$'))
        {
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
        }
        else
        {
          // dont handle this case for now 
          return errorHere("two '$' in a row is currently unrecognized");
        }
      }
      else if (at('('))
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
      {
        initCurt();
        while (not at('\n') and not eof())
          advance();

        finishCurt(LuaLine);

        if (not eof())
          advance();

        // Remove whitespace before lua lines to preserve formatting 
        // NOTE(sushi) this is a large reason why the lexer is not 
        //             token-stream based anymore!
        if (tokens.len() != 0)
        {
          Token* last = tokens.arr + tokens.len() - 2;
          str raw = last->getRaw(source);
          while (isspace(raw.ptr[last->len-1]) && 
                 raw.ptr[last->len-1] != '\n')
            last->len -= 1;
        }
      }
    }
    else if (at('@'))
    {
      initCurt();
      advance();
      curt.macro_indent_loc = trailing_space_start;
      curt.macro_indent_len = trailing_space_len;
      Token::Kind macro_kind = MacroSymbol;
      if (at('@'))
      {
        advance();
        macro_kind = MacroSymbolImmediate;
      }
      finishCurt(macro_kind);

      skipWhitespace();
      initCurt();

      if (not atFirstIdentifierChar())
        return errorHere("expected an identifier of a macro after '@'");
      
      // NOTE(sushi) allow '.' so that we can index tables through macros, eg.
      //             $ local lib = {}
      //             $ lib.func = function() print("hi!") end
      //
      //             @lib.func()
      // TODO(sushi) this could maybe be made more advanced by allowing 
      //             arbitrary whitespace between the dots.
      b8 found_colon = false;
      while (atIdentifierChar() or 
             at('.') or 
             at(':'))
      {
        if (at(':'))
        {
          // Peek ahead and see if an identifier character follows. If not 
          // then assume this is some document syntax being used directly
          // after a macro call.
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

      if (curt.getRaw(source).hash() == "child.style"_hashed)
      {
        ERROR("child style\n");
      }

      skipWhitespace();

      switch (current())
      {
      case '(':
        advance();
        skipWhitespace();

        if (at(')'))
        {
          // dont create a token for empty macro args
          advance();
          initCurt();
          break;
        }

        {
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
            skipWhitespace();
            if (reset_curt)
              initCurt();
          }
        }

        advance();
        initCurt();
        break;

      case '"':
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
        break;

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

      }
    }
    else
    {
      
    }
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Lexer::initCurt()
{
  curt.kind = Token::Kind::Eof;
  curt.loc = current_offset;
}

/* ----------------------------------------------------------------------------
 */
void Lexer::finishCurt(Token::Kind kind, s32 len_offset)
{
  s32 len = len_offset + current_offset - curt.loc;
  if (len > 0)
  {
    curt.kind = kind;
    curt.len = len;
    tokens.push(curt);

    TRACE("finished ", curt, "\n");
  }
}

}
