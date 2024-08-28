/*
 *  LSP server stucture and api
 *
 */

#ifndef _lpp_lsp_server_h
#define _lpp_lsp_server_h

#include "iro/common.h"
#include "iro/unicode.h"
#include "iro/json/types.h"

#include "generated/lsp.h"

#include "filemap.h"

#include "lpp.h"

using namespace iro;

namespace lsp
{

/* ============================================================================
 */
struct Server
{
  Lpp* lpp = nullptr;

  str root_path = nil;
  str root_uri = nil;

  InitializeParams* init_params = nullptr;
  mem::LenientBump init_params_allocator = {};

  FileMap open_files;

  b8   init(Lpp* lpp);
  void deinit();

  b8 loop();
};

} // namespace lsp

DefineNilValue(lsp::Server, {}, { return x.lpp == nullptr; });

#endif // _lpp_lsp_server_h
