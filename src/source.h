/*
 *  Central representation of some source given to lpp for preprocessing.
 *  
 *  Handles translating source locations (byte offsets) to other information
 *  such as raw strings and line+columns as well as caching useful information.
 */

#ifndef _lpp_source_h
#define _lpp_source_h

#include "common.h"
#include "unicode.h"
#include "containers/array.h"
#include "io/io.h"

using namespace iro;

/* ================================================================================================ Source
 */
struct Source 
{
	str name;

	// A cache of this source's content.
	// TODO(sushi) figure out when we're given memory that we can just 
	//             keep a view over.
	//             or maybe later stuff should be designed to get 
	//             content on demand so we dont store so much memory?
	io::Memory cache;

	// Byte offsets into this source where we find newlines.
	b8 line_offsets_calculated;
	Array<u64> line_offsets;

    /* ============================================================================================ Source::Loc
     */
	struct Loc
	{	
		u64 line;
		u64 column;
	};


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */


	b8   init(str name);
	void deinit();

	// writes to the cache and handles some state such as if 
	// line offsets are calculated
	b8 writeCache(Bytes slice);

	// caches line offsets in the cached buffer
	b8 cacheLineOffsets();

	str getStr(u64 offset, u64 length);

	// translate a byte offset into this source to 
	// a line and column
	Loc getLoc(u64 offset);
};

#endif // _lpp_source_h
