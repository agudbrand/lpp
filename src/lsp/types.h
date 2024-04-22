/*
 *  Types representing LSP data structures.
 */

#ifndef _lpp_lsp_types_h
#define _lpp_lsp_types_h

#include "common.h"
#include "unicode.h"

namespace lsp
{

/* ================================================================================================ lsp::Message
 */
struct Message
{
	// always "2.0" in lsp
	str jsonrpc;
};

/* ================================================================================================ lsp::RequestMessage
 */
struct RequestMessage : public Message
{
	// Request id.
	union 
	{
		s64 integer;
		str string;
	} id;

	// Method to be invoked
	str method;

	// Method params
	
};

} // namespace lsp

#endif // _lpp_lsp_types_h
