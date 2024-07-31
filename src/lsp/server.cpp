#include "server.h"

#include "iro/io/io.h"
#include "iro/fs/file.h"
#include "iro/json/parser.h"

#include "signal.h"
#include "unistd.h"
#include "sys/types.h"

static auto logger = 
  Logger::create("lsp.server"_str, Logger::Verbosity::Trace);

namespace lsp
{

using VKind = json::Value::Kind;

/* ----------------------------------------------------------------------------
 */
b8 Server::init(Lpp* lpp)
{
  assert(lpp);
  this->lpp = lpp;
  return true;
}

/* ----------------------------------------------------------------------------
 */
void Server::deinit()
{
  *this = nil;
}

/* ----------------------------------------------------------------------------
 */
b8 parseTestFile(Server* server)
{
  auto f = fs::File::from("test.json"_str, fs::OpenFlag::Read);
  if (isnil(f))
    return false;
  defer { f.close(); };

  json::JSON json;
  json::Parser parser;

  jmp_buf failjmp;
  if (setjmp(failjmp))
    return false;

  if (!parser.init(&f, &json, "test.json"_str, &failjmp))
    return false;
  defer { parser.deinit(); json.deinit(); };

  if (!parser.start())
    return false;

  json.prettyPrint(&fs::stdout);

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Server::loop()
{
  io::Memory buffer;
  buffer.open();

  return parseTestFile(this);

  // TRACE("pid is: ", getpid(), "\n");
  // raise(SIGSTOP);

  for (;;)
  {
    buffer.clear();

    // Read till we reach the end of a message.
    for (;;)
    {
      auto reserved = buffer.reserve(128);
      auto bytes_read = fs::stdin.read(reserved);
      if (bytes_read == -1)
        return false;
      buffer.commit(bytes_read);
      if (reserved.end()[-1] == 0)
        break;
    }

    TRACE(buffer.asStr(), "\n");
    
    json::JSON json;
    json::Parser parser;

    // TODO(sushi) just pass stdin into this
    // TODO(sushi) dont init the JSON in the parser, do it externally
    if (!parser.init(&buffer, &json, "lspinput"_str))
      return false;
    defer { parser.deinit(); json.deinit(); };

    if (!parser.start())
      return false;

    buffer.clear();
    json.prettyPrint(&buffer);

    fs::stderr.write(buffer.asStr());
  }

  return true;
}

} // namespace lsp
