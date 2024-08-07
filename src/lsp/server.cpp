#include "server.h"

#include "iro/io/io.h"
#include "iro/fs/file.h"
#include "iro/json/parser.h"

#include "signal.h"
#include "unistd.h"
#include "sys/types.h"

namespace lsp
{

// static auto logger = 
//   Logger::create("lsp.server"_str, Logger::Verbosity::Trace);

/* ----------------------------------------------------------------------------
 */
b8 Server::init(Lpp* lpp)
{
  DEBUG("init\n");
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
template<typename... Args>
static b8 processError(Args... args)
{
  ERROR(args...);
  return false;
}

/* ----------------------------------------------------------------------------
 */
static str tryGetStringMember(json::Object* o, str name)
{
  json::Value* v = o->findMember(name);
  if (!v)
    return nil;
  if (v->kind != json::Value::Kind::String)
    return nil;
  return v->string;
}

/* ----------------------------------------------------------------------------
 */
static json::Object* tryGetObjectMember(json::Object* o, str name)
{
  json::Value* v = o->findMember(name);
  if (!v)
    return nullptr;
  if (v->kind != json::Value::Kind::Object)
    return nullptr;
  return &v->object;
}

/* ----------------------------------------------------------------------------
 *  Processes the given request and produces a response in the given json.
 */
static b8 processRequest(
    Server* server, 
    json::Object* request, 
    json::Object* response,
    json::JSON* json) // the JSON object containing the response
{
  using namespace json;

  Value* id = request->findMember("id"_str);
  if (!id)
    return processError("lsp request missing id\n");

  Value* response_id = json->newValue(id->kind);
  if (!response_id)
    return processError("failed to create response id value\n");
  response->addMember("id"_str, response_id);

  switch (id->kind)
  {
  case Value::Kind::String:
    response_id->string = id->string.allocateCopy(&json->string_buffer);
    break;
  case Value::Kind::Number:
    response_id->number = id->number;
    break;
  default:
    return processError(
        "invalid value kind ", getValueKindString(id->kind), 
        " encountered in lsp request\n");
  }
  
  str method = tryGetStringMember(request, "method"_str);
  if (isnil(method))
    return processError("lsp request missing method\n");

  switch (method.hash())
  {
  case "initialize"_hashed:
    {
      Object* params = tryGetObjectMember(request, "params"_str);
      if (!params)
        return processError("lsp request missing params\n");

      if (!server->init_params_allocator.init())
        return processError(
            "failed to initialize InitializeParams allocator\n");

      if (!deserializeInitializeParams(
            &server->init_params_allocator,
            params,
            &server->init_params))
        return processError("failed to deserialize InitializeParams\n");
    }
    break;
  }

  return true;
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
  if (!json.init())
    return processError("failed to init json object for test file\n");

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

  json::JSON response_json;
  if (!response_json.init())
    return processError("failed to initialize response json object\n");

  response_json.root = response_json.newValue(json::Value::Kind::Object);
  response_json.root->init();

  if (!processRequest(
        server,
        &json.root->object,
        &response_json.root->object,
        &response_json))
    return false;

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Server::loop()
{
  io::Memory buffer;
  buffer.open();

  return parseTestFile(this);

  // DEBUG("pid is: ", getpid(), "\n");
  // raise(SIGSTOP);

  DEBUG("entering server loop\n");

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
