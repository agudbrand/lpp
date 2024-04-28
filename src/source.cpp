#include "source.h"
#include "unicode.h"

/* ------------------------------------------------------------------------------------------------ Source::init
 */
b8 Source::init(str name)
{
	if (!cache.open())
		return false;
	line_offsets = Array<s32>::create();
	return true;
}

/* ------------------------------------------------------------------------------------------------ Source::deinit
 */
void Source::deinit()
{
	cache.close();
	line_offsets.destroy();
	*this = {};
}

/* ------------------------------------------------------------------------------------------------ Source::get_str
 */
str Source::get_str(s32 loc, s32 len)
{
	return {cache.buffer + loc, len};
}

/* ------------------------------------------------------------------------------------------------ Source::get_loc
 */
Source::Loc Source::get_loc(s32 loc)
{
	s32 l = 0, 
		m = 0, 
		r = line_offsets.len();

	while (l <= r)
	{
		m = l + (r - l) / 2;
		s32 cur = line_offsets[m];
		if (cur < loc)
			l = m+1;
		else if (cur > loc)
			r = m-1;
		else
			break;
	}

	s32 column = 0;
	s32 offset = line_offsets[m];
	for (;;)
	{
		utf8::Codepoint c = utf8::decode_character(cache.buffer + offset, 4);
		offset += c.advance;
		column += 1;
		if (offset > loc)
			break;
	}

	return {m+1, column};
}

