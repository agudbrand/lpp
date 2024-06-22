#include "lex.h"

#include "iro/logger.h"

#include "ctype.h"
#include "assert.h"

static Logger logger = 
  Logger::create("lpp.lexer"_str, Logger::Verbosity::Notice);



/* ----------------------------------------------------------------------------
 */
u32 Lexer::current() { return current_codepoint.codepoint; }

b8 Lexer::at(u8 c) { return current() == c; }
b8 Lexer::eof()    { return at_end || at(0) || isnil(current_codepoint); }

b8 Lexer::atFirstIdentifierChar() { return isalpha(current()) || at('_'); }
b8 Lexer::atIdentifierChar() { return atFirstIdentifierChar() || 
                                      isdigit(current()); }

/* ----------------------------------------------------------------------------
 *  Returns false if we've reached the end of the buffer.
 */
b8 Lexer::readStreamIfNeeded()
{
  if (current_offset == source->cache.len)
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
      source->cache.buffer + current_offset, 
      source->cache.len - current_offset);

  return notnil(current_codepoint);
}

/* ----------------------------------------------------------------------------
 */
void Lexer::advance(s32 n)
{
  for (s32 i = 0; i < n; i++)
  {
    current_offset += current_codepoint.advance;

        if (!readStreamIfNeeded())
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
void Lexer::skipWhitespace()
{
  while (isspace(current()))
    advance();
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
  readStreamIfNeeded();
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

    while (not at('@') and
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
      advance();
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
          while (isspace(raw.bytes[last->len-1]) && 
                 raw.bytes[last->len-1] != '\n')
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
      finishCurt(MacroSymbol);

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
      while (atIdentifierChar() or at('.'))
        advance();
      
      finishCurt(MacroIdentifier);
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

        for (;;)
        {
          initCurt();

          while (not at(',') and 
               not at(')') and 
               not eof())
            advance();

          if (eof())
            return errorAtToken(curt, 
                "unexpected end of file while consuming macro arguments");

          finishCurt(MacroArgumentTupleArg);

          if (at(')'))
            break;

          advance();
          skipWhitespace();
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
        finishCurt(MacroArgumentString);
        advance();
        break;
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


