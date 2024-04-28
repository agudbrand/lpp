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

	// Byte offsets into this source.
	Array<u64> line_offsets;

	struct Loc
	{	
		u64 line;
		u64 column;
	};


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */


	b8   init(str name);
	void deinit();

	str get_str(u64 offset, u64 length);
	Loc get_loc(u64 offset);
};

#endif // _lpp_source_h
