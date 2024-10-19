/*
 *  LSP server stucture and api
 *
 */

#ifndef _lpp_lsp_server_h
#define _lpp_lsp_server_h

#if 0

#include "iro/common.h"
#include "iro/unicode.h"
#include "iro/json/types.h"

#include "generated/lsp.h"

#include "filemap.h"

#include "lpp.h"

using namespace iro;

namespace lpp::lsp
{

/* ============================================================================
 */
struct Server
{
  Lpp* lpp = nullptr;

  String root_path = nil;
  String root_uri = nil;

  InitializeParams* init_params = nullptr;
  mem::LenientBump init_params_allocator = {};

  FileMap open_files;

  b8   init(Lpp* lpp);
  void deinit();

  b8 loop();
};

} // namespace lsp

DefineNilValue(lpp::lsp::Server, {}, { return x.lpp == nullptr; });

#endif

#endif // _lpp_lsp_server_h
