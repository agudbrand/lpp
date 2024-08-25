#include "server.h"

#include "iro/io/io.h"
#include "iro/fs/file.h"
#include "iro/json/parser.h"

#include "signal.h"
#include "unistd.h"
#include "sys/types.h"

#include "ctype.h"
#include "cstdlib"

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
 *  Fills out a ServerCapabilities struct with what the lsp server 
 *  currently supports.
 */
static void writeServerCapabilities(ServerCapabilities* out)
{
  // Just use utf16 for now since its required to be supported.
  out->positionEncoding = PositionEncodingKind::UTF16;

  // Not sure yet.
  out->textDocumentSync = TextDocumentSyncKind::Full;

  // Don't know what exactly this is yet.
  out->completionProvider.completionItem.labelDetailsSupport = false;

  // No extra trigger or commit characters for now.
  out->completionProvider.triggerCharacters = nil;
  out->completionProvider.allCommitCharacters = nil;

  // Not sure what this is yet.
  out->completionProvider.resolveProvider = false;

  // Not yet supported stuff.
  out->hoverProvider = false;
  out->signatureHelpProvider.triggerCharacters = nil;
  out->signatureHelpProvider.retriggerCharacters = nil;
  out->declarationProvider = false;
  out->definitionProvider = false;
  out->typeDefinitionProvider = false;
  out->referencesProvider = false;
  out->documentHighlightProvider = false;
  out->documentSymbolProvider = false;
  out->codeActionProvider = false;
  out->codeLensProvider.resolveProvider = false;
  out->documentLinkProvider.resolveProvider = false;
  out->colorProvider = false;
  out->documentFormattingProvider = false;
  out->documentRangeFormattingProvider = false;
  out->documentOnTypeFormattingProvider.firstTriggerCharacter = nil;
  out->documentOnTypeFormattingProvider.moreTriggerCharacter = nil;
  out->renameProvider = false;
  out->foldingRangeProvider = false;
  out->executeCommandProvider.commands = nil;
  out->selectionRangeProvider = false;
  out->linkedEditingRangeProvider = false;
  out->callHierarchyProvider = false;
  out->monikerProvider = false;
  out->typeHierarchyProvider = false;
  out->inlineValueProvider = false;
  out->inlayHintProvider = false;
  out->workspaceSymbolProvider = false;

  // Workspace folders stuff.
  {
    out->workspace.workspaceFolders.supported = false;
    out->workspace.workspaceFolders.changeNotifications.setAsBoolean(false); 
  }

  // File operations
  {
    // Just a buncha empty arrays for now. Might cause problems.
    out->workspace.fileOperations.didCreate.filters.init();
    out->workspace.fileOperations.willCreate.filters.init();
    out->workspace.fileOperations.didRename.filters.init();
    out->workspace.fileOperations.willRename.filters.init();
    out->workspace.fileOperations.didDelete.filters.init();
    out->workspace.fileOperations.willDelete.filters.init();
  }

  // Diagnostics stuff.
  {
    out->diagnosticProvider.identifier = nil;
    out->diagnosticProvider.interFileDependencies = false;
    out->diagnosticProvider.workspaceDiagnostics = false;
  }

  // Semantic tokens stuff.
  {
    // Empty arrays for now.
    out->semanticTokensProvider.legend.tokenTypes.init();
    out->semanticTokensProvider.legend.tokenModifiers.init();
    out->semanticTokensProvider.range.setAsBoolean(false);
    out->semanticTokensProvider.full.setAsBoolean(false);
  }
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
  default:
    return processError("unhandled method in processRequest: ", method, "\n");
  case "initialize"_hashed:
    {
      Object* params = tryGetObjectMember(request, "params"_str);
      if (!params)
        return processError("lsp request missing params\n");

      if (!server->init_params_allocator.init())
        return processError(
            "failed to initialize InitializeParams allocator\n");

      if (!server->init_params.deserialize(
            params, 
            &server->init_params_allocator))
        return processError("failed to deserialize InitializeParams\n");

      InitializeResult init_result;
      init_result.serverInfo.name = "lpplsp"_str;
      init_result.serverInfo.version = "0.0.1"_str;
      writeServerCapabilities(&init_result.capabilities);

      Value* response_result = json->newValue(Value::Kind::Object);
      if (!response_result)
        return processError("failed to create response result object\n");
      if (!response_result->init())
        return processError("failed to initialize response result object\n");
      response->addMember("result"_str, response_result);

      mem::LenientBump init_result_allocator;
      if (!init_result_allocator.init())
        return processError(
            "failed to initialize allocator for InitializeResult "
            "serialization\n");
      defer { init_result_allocator.deinit(); };

      Object* result_obj = &response_result->object;

      if (!init_result.serialize(
            json, 
            &response_result->object, 
            &init_result_allocator))
        return processError("failed to serialize InitializeResult\n");

      json->prettyPrint(&fs::stdout);
    }
    break;
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
static b8 processNotification(Server* server, json::Object* notification)
{
  using namespace json;

  str method = tryGetStringMember(notification, "method"_str);
  if (isnil(method))
    return processError("lsp request missing method\n");

  switch (method.hash())
  {
  default:
    return processError(
        "unhandled method in processNotification: ", method, "\n");
  case "initialized"_hashed:
    break;
  case "textDocument/didOpen"_hashed:
    {
      Object* params = tryGetObjectMember(notification, "params"_str);
      if (!params)
        return processError("lsp request missing params\n");

      mem::LenientBump allocator;
      if (!allocator.init())
        return processError("failed to initialize allocator\n");
      defer { allocator.deinit(); };

      DidOpenTextDocumentParams did_open_params;
      if (!did_open_params.deserialize(params, &allocator))
        return processError(
            "failed to deserialize textDocument/didOpen params\n");
      
      str uri = did_open_params.textDocument.uri;
      str file = uri.sub("file:///"_str.len-1);

      INFO("opened file ", file, "\n");
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

  // return parseTestFile(this);

  // DEBUG("pid is: ", getpid(), "\n");
  // raise(SIGSTOP);

  DEBUG("entering server loop\n");

  for (;;)
  {
    buffer.clear();

    const s32 header_buffer_size = 255;

    u8 header_buffer[header_buffer_size] = {};
    u8* cursor = header_buffer;

    Bytes reserved_space = {};

    for (;;)
    {
      if (cursor > header_buffer + header_buffer_size)
      {
        ERROR("content header is too long\n");
        return false;
      }

      fs::stdin.read({cursor, 1});
      if (*cursor == ':')
      {
        str fieldname = str::from(header_buffer,cursor);
        if (fieldname == "Content-Length"_str)
        {
          cursor = header_buffer;
          fs::stdin.read({cursor, 1});
          while (isspace(*cursor))
            fs::stdin.read({cursor, 1});
          while (*cursor != '\r')
          {
            cursor += 1;
            fs::stdin.read({cursor, 1});
          }
          s32 content_length = strtoul((char*)header_buffer, nullptr, 0);
          reserved_space = buffer.reserve(content_length);
          buffer.commit(content_length);
          *header_buffer = '\r';
          cursor = header_buffer + 1;
        }
      }
      else if (header_buffer[0] == '\r' && header_buffer[1] == '\n' &&
               header_buffer[2] == '\r' && header_buffer[3] == '\n')
        break;
      else
        cursor += 1;
    }

    fs::stdin.read(reserved_space);

    {
      auto f = fs::File::from("lastmessage.json"_str, 
            fs::OpenFlag::Create
          | fs::OpenFlag::Write
          | fs::OpenFlag::Truncate);

      f.write(reserved_space);
      f.close();
    }

    // TRACE(buffer.asStr(), "\n");

    json::JSON json;
    if (!json.init())
      return processError(
          "failed to initialize json object for parsing incoming data\n");
    defer { json.deinit(); };

    json::Parser parser;
    // TODO(sushi) just pass stdin into this
    if (!parser.init(&buffer, &json, "lspinput"_str))
      return processError(
          "failed to initialize json parser for incoming data\n");
    defer { parser.deinit(); };

    if (!parser.start())
      return processError("failed to parse JSON message\n");

    buffer.clear();
    json.prettyPrint(&buffer);

    // TRACE(buffer.asStr());

    if (json.root->object.findMember("id"_str))
    {
      // This is a request.
      json::JSON response_json;
      if (!response_json.init())
        return processError("failed to initialize response json object\n");

      response_json.root = response_json.newValue(json::Value::Kind::Object);
      response_json.root->init();

      if (!processRequest(
            this,
            &json.root->object,
            &response_json.root->object,
            &response_json))
        return false;

      buffer.clear();
      response_json.prettyPrint(&buffer);
      // TRACE(buffer.asStr());
      io::formatv(&fs::stdout, "Content-Length: ", buffer.len, "\r\n\r\n");
      fs::stdout.write(buffer.asStr());
    }
    else
    {
      // This is a notification.
      if (!processNotification(
            this,
            &json.root->object))
        return false;
    }
  }

  return true;
}

} // namespace lsp
