/*
 *  URI parsing and generating api
 */

#ifndef _lpp_uri_h
#define _lpp_uri_h

#include "common.h"
#include "unicode.h"
#include "logger.h"

/* ================================================================================================ URI
 *  A URI and its parts. This always copies whatever is given to it and owns the memory it views.
 */
struct URI
{
	dstr scheme;
	dstr authority;
	dstr body;

	b8   init();
	void deinit();

	void reset();

	// parses the given string into a URI 
	//
	static b8 parse(URI* uri, str s);

	b8 encode();
	b8 decode();
};

#endif // _lpp_uri_types_h
