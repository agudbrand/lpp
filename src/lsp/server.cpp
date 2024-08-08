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
    out->workspace.workspaceFolders.changeNotifications_kind = 
      json::Value::Kind::Boolean;
    out->workspace.workspaceFolders.changeNotifications_boolean = false;
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
    out->semanticTokensProvider.range_kind = json::Value::Kind::Boolean;
    out->semanticTokensProvider.range_boolean = false;
    out->semanticTokensProvider.full_kind = json::Value::Kind::Boolean;
    out->semanticTokensProvider.full_boolean = false;
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

      ServerCapabilities server_capabilities = {};
      writeServerCapabilities(&server_capabilities);

      Value* response_result = json->newValue(Value::Kind::Object);
      if (!response_result)
        return processError("failed to create response result object\n");
      if (!response_result->init())
        return processError("failed to initialize response result object\n");
      response->addMember("result"_str, response_result);

      mem::LenientBump server_capabilities_allocator;
      if (!server_capabilities_allocator.init())
        return processError(
            "failed to initialize allocator for ServerCapabilities "
            "serialization\n");
      defer { server_capabilities_allocator.deinit(); };

      if (!serializeServerCapabilities(
            json,
            &response_result->object,
            server_capabilities,
            &server_capabilities_allocator))
        return processError("failed to serialize ServerCapabilities\n");

      json->prettyPrint(&fs::stdout);
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
