#include "parser.h"
#include "iro/linemap.h"
#include "iro/fs/file.h"

#include "ctype.h"

namespace lpp
{

static Logger logger = 
  Logger::create("lpp.parser"_str, Logger::Verbosity::Notice);

/* ----------------------------------------------------------------------------
 */
b8 Parser::init(
    Source* src,
    io::IO* instream, 
    io::IO* outstream)
{
  assert(instream && outstream);

  TRACE("initializing on stream '", src->name, "'\n");

  tokens = Array<Token>::create();
  locmap = Array<LocMapping>::create();
  in = instream;
  out = outstream;
  source = src;

  if (!lexer.init(in, src))
    return false;

  if (setjmp(lexer.err_handler))
  {
    lexer.deinit();
    return false;
  }

  if (!lexer.run())
    return false;

  curt = lexer.tokens.arr-1;

  src->cacheLineOffsets();
  return true;
}

/* ----------------------------------------------------------------------------
 */
void Parser::deinit()
{
  lexer.deinit();
  *this = {};
}

/* ----------------------------------------------------------------------------
 */
void Parser::writeTokenSanitized()
{
  // Write out the current token with its control characters 
  // sanitized to lua's representation of them.
  for (u8 c : getRaw())
  {
    if (iscntrl(c))
      writeOut("\\"_str, c);
    else if (c == '"')
      writeOut("\\\""_str);
    else if (c == '\\')
      writeOut("\\\\"_str);
    else 
      writeOut((char)c);
  }
}

/* ----------------------------------------------------------------------------
 */
template<typename... T>
void Parser::writeOut(T... args)
{
  bytes_written += io::formatv(out, args...);
}

/* ----------------------------------------------------------------------------
 */
b8 Parser::run()
{
  TRACE("begin\n");

  nextToken();

  for (;;)
  {
    using enum Token::Kind;

    if (at(Eof))
    {
      TRACE("encountered EOF\n");
      break;
    }

    switch (curt->kind)
    {
      case Invalid:
        return false;

      case DollarSign: // included for lsp stuff
        nextToken();
        break;

      case Document:
        TRACE("placing document text: '", 
              io::SanitizeControlCharacters(getRaw()), "'\n");
        {
          str raw = getRaw();
          u64 from = curt->loc;
          locmap.push({.from = bytes_written, .to = curt->loc});
          for (s32 i = 0; i < raw.len; ++i)
          {
            if (i && raw.bytes[i-1] == '\n' && i != raw.len-1)
              locmap.push({.from = bytes_written+i, .to = curt->loc+i});
          }
        }

        writeOut("__metaenv.doc("_str, curt->loc, ",\""_str);
        writeTokenSanitized();
        writeOut("\")\n"_str);
        nextToken();
        break;

      case LuaBlock:
        TRACE("placing lua block: '", 
              io::SanitizeControlCharacters(getRaw()), "'\n");
        {
          str raw = getRaw();
          locmap.push({.from = bytes_written, .to = curt->loc});
          for (s32 i = 0; i < raw.len; ++i)
          {
            if (i && raw.bytes[i-1] == '\n' && i != raw.len-1)
              locmap.push({.from = bytes_written+i, .to = curt->loc+i});
          }
        }
        writeOut(getRaw(), "\n");
        nextToken();
        break;

      case LuaLine:
        TRACE("placing lua line: '", 
              io::SanitizeControlCharacters(getRaw()), "'\n");
        locmap.push({.from = bytes_written, .to = curt->loc-1});
        writeOut(getRaw(), "\n");
        nextToken();
        break;

      case LuaInline:
        TRACE("placing lua inline: '", 
              io::SanitizeControlCharacters(getRaw()), "'\n");
        locmap.push({.from = bytes_written, .to = curt->loc});
        writeOut("__metaenv.val("_str, curt->loc, ",", getRaw(), ")\n"_str);
        nextToken();
        break;

      case MacroSymbol:
        TRACE("encountered macro symbol\n");
        locmap.push({.from = bytes_written, .to = curt->loc});
        writeOut(
          "__metaenv.macro("_str, curt->loc, ",\"", 
          source->getStr(curt->macro_indent_loc, curt->macro_indent_len), 
          "\",");

        nextToken(); // identifier

        locmap.push({.from = bytes_written, .to = curt->loc});
        writeOut('"', getRaw(), "\",", getRaw());

        nextToken();
        if (at(MacroArgumentTupleArg))
        {
          for (;;)
          {
            locmap.push({.from = bytes_written, .to = curt->loc});
            // NOTE(sushi) used to use this MacroPart stuff here 
            //             but finding that it makes working with macro
            //             arguments too unintuitive. Later once we get to
            //             needing to use the mappings these would provide
            //             we should provide some sorta lpp.macro api whose
            //             use makes it apparent that the macro arguments
            //             are special types, not just strings.
            // writeOut(',', 
            //   "__metaenv.lpp.MacroPart.new(",
            //   '"', source->name, '"', ',',
            //   curt->loc,              ',',
            //   curt->loc + curt->len,  ',',
            //   '"');
            writeOut(',', '"');
            writeTokenSanitized();
            // writeOut('"', ')');
            writeOut('"');
            nextToken();
            if (not at(MacroArgumentTupleArg))
              break;
            // writeOut(',');
          }
        }
        else if (at(MacroArgumentString))
        {
          locmap.push({.from = bytes_written, .to = curt->loc});
          writeOut(',', '"', getRaw(), '"');
          nextToken();
        }

        writeOut(")\n"_str);
        break;
    }
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Parser::nextToken()
{
  curt += 1;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Parser::at(Token::Kind kind)
{
  return curt->kind == kind;
}

/* ----------------------------------------------------------------------------
 */
str Parser::getRaw()
{
  return curt->getRaw(source);
}

}
