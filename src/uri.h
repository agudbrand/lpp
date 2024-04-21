/*
 *  URI type and function for parsing a string into it.
 */

#ifndef _lpp_uri_h
#define _lpp_uri_h

#include "common.h"
#include "unicode.h"

namespace uri
{

/* ================================================================================================ uri::URI
 */
struct URI
{
	str scheme;
	str authority;
	str path;
	str query;
	str fragment;
};

/* ================================================================================================ uri::Parser
 */
struct Parser
{
	URI* uri;

	str string;
	str cursor;


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */


	b8   init(URI* uri, str s);
	void deinit();

	b8 start();

private:

	b8 scheme();
	b8 hier_part();
	b8 query();
	b8 fragment();
	b8 authority();

	b8 path_abempty();
	b8 path_absolute();
	b8 path_rootless();
	b8 path_empty();
	b8 pchar();

	b8 user_info();

	b8 host();

	b8 ip_literal();

	b8 ipv4address();
	b8 reg_name();


};

} // namespace uri

#endif // _lpp_uri_types_h
