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
static b8 tryGetBoolMember(json::Object* o, str member, b8* out_bool)
{
  json::Value* mem = o->findMember(member);
  if (!mem)
    return false;

  if (mem->kind == VKind::True)
  {
    *out_bool = true;
    return true;
  }
  
  if (mem->kind == VKind::False)
  {
    *out_bool = false;
    return true;
  }

  return false;
}

static b8 tryGetBoolMember(json::Value* v, str member, b8* out_bool)
{
  if (v->kind != VKind::Object)
    return false;
  return tryGetBoolMember(&v->object, member, out_bool);
}

/* ----------------------------------------------------------------------------
 */
static json::Object* tryGetObjectMember(json::Object* obj, str member)
{
  json::Value* mem = obj->findMember(member);
  if (!mem)
    return nullptr;
  return &mem->object;
}

/* ----------------------------------------------------------------------------
 */
static void processClientTextDocumentCapabilities(
    TextDocumentCapabilities* capabilities,
    json::Object* clientcap)
{
#define helper(loc,str,mem,mem2) \
  tryGetBoolMember(loc,GLUE(str,_str),&capabilities->mem.mem2)

#define mem(name) \
  if (json::Object* member = tryGetObjectMember(clientcap, GLUE(name,_str))) \
  {

  if (json::Object* sync = 
      tryGetObjectMember(clientcap, "synchronization"_str))
  {
#define z(x,y) helper(sync,x,synchronization,y)
    z("dynamicRegistration", dynamic_registration);
    z("willSave", will_save);
    z("willSaveWaitUntil", will_save_wait_until);
    z("didSave", did_save);
#undef z
  }

  if (json::Object* completion = 
      tryGetObjectMember(clientcap, "completion"_str))
  {
#define z(x,y) helper(completion,x,completion,y)
    z("")
      
#undef z

  }
}

/* ----------------------------------------------------------------------------
 */
static void processClientWorkspaceCapability(
    ClientCapabilities* capabilities,
    json::Value* capability)
{
  switch (capability->string.hash())
  {
#define refsup(x,y) \
    case GLUE(x,_hashed): \
      tryGetBoolMember( \
        capability, \
        "refreshSupport"_str, \
        &capabilities->refresh_support.y); \
        break; 
  refsup("semanticTokens", semantic_tokens);
  refsup("codeLenss",      code_lens);
  refsup("inlineValue",    inline_value);
  refsup("inlayHint",      inlay_hint);
  refsup("diagnostics",    diagnostics);
#undef refsup

  case "fileOperations"_hashed:
#define fo(x,y) \
    tryGetBoolMember( \
      capability, \
      GLUE(x,_str), \
      &capabilities->fileop.y);
  fo("dynamicRegistration", dynamic_registration);
  fo( "didCreate",  did_create);
  fo("willCreate", will_create);
  fo( "didRename",  did_rename);
  fo("willRename", will_rename);
  fo( "didDelete",  did_delete);
  fo("willDelete", will_delete);
#undef fo
  }
}

/* ----------------------------------------------------------------------------
 */
static void gatherClientCapabilities(
    Server*       server, 
    json::Object* capabilities)
{
  for (auto category : capabilities->members)
  {
    switch (category.hash)
    {
    case "general"_hashed:
      for (auto general_capability : category.value->object.members)
      {
        switch (general_capability.hash)
        {
        case "positionEncodings"_hashed:
          {
            // prefer utf8, maybe
            auto encodings = &general_capability.value->array;
            if (0 == encodings->values.len())
            {
              // have to assume utf16
              position_encoding = PositionEncodingKind::UTF16;
            }
            else
            {
              for (auto encoding : encodings->values)
              {
                PositionEncodingKind kind;
                switch (encoding->string.hash())
                {
                case "utf-8"_hashed:  
                  kind = PositionEncodingKind::UTF8; break;
                case "utf-16"_hashed: 
                  kind = PositionEncodingKind::UTF16; break;
                case "utf-32"_hashed:
                  kind = PositionEncodingKind::UTF32; break;
                }
              }
            }
          }
          break;
        }
      }
      break;
    }
  }
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

  switch (json.root->kind)
  {
  case json::Value::Kind::Object:
    {
      json::Object* obj = &json.root->object;
      if (json::Value* val = obj->findMember("clientInfo"_str))
      {
        json::Object* client_info = &val->object;

      }
    }
    break;
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Server::loop()
{
  io::Memory buffer;
  buffer.open();

  return parseTestFile();

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
