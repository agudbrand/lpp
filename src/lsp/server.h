/*
 *  LSP server stucture and api
 *
 */

#ifndef _lpp_lsp_server_h
#define _lpp_lsp_server_h

#include "iro/common.h"
#include "iro/unicode.h"

#include "lpp.h"

namespace lsp
{

/* ================================================================================================ lsp::Server
 */
struct Server
{
	Lpp lpp;

	b8   init();
	void deinit();

	b8 loop();
};

} // namespace lsp

#endif // _lpp_lsp_server_h
