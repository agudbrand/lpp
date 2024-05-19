#include "source.h"

#include "lpp.h"

#include "iro/unicode.h"

/* ------------------------------------------------------------------------------------------------ Source::init
 */
b8 Source::init(str name)
{
	if (!cache.open())
		return false;
	line_offsets = Array<u64>::create();
	line_offsets_calculated = false;
	this->name = name;
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

/* ------------------------------------------------------------------------------------------------ Source::writeCache
 */
b8 Source::writeCache(Bytes slice)
{
	line_offsets_calculated = false;
	cache.write(slice);
	return true;
}

/* ------------------------------------------------------------------------------------------------ Source::cacheLineOffsets
 */
b8 Source::cacheLineOffsets()
{
	if (line_offsets_calculated)
		return true;

	// TODO(sushi) continue calculated offsets if there are any
	line_offsets.destroy();
	line_offsets = Array<u64>::create();

	line_offsets.push(0);

	str s = cache.asStr();
	for (s64 i = 0; i < s.len; i++)
	{
		if (s.bytes[i] == '\n')
			line_offsets.push(i+1);
	}

	line_offsets_calculated = true;
	return true;
}

/* ------------------------------------------------------------------------------------------------ Source::getStr
 */
str Source::getStr(u64 loc, u64 len)
{
	return {cache.buffer + loc, len};
}

/* ------------------------------------------------------------------------------------------------ Source::getLoc
 */
Source::Loc Source::getLoc(u64 loc)
{
	u64 l = 0, 
		m = 0, 
		r = line_offsets.len() - 1;

	while (l <= r)
	{
		m = l + (r - l) / 2;
		if (!m)
			break;
		u64 cur = line_offsets[m];
		if (cur < loc)
			l = m+1;
		else if (cur > loc)
			r = m-1;
		else
			break;
	}

	if (line_offsets[m] > loc)
		m -= 1;

	u64 column = 1;
	u64 offset = line_offsets[m];
	for (;;)
	{
		if (offset >= loc)
			break;
		utf8::Codepoint c = utf8::decodeCharacter(cache.buffer + offset, 4);
		offset += c.advance;
		column += 1;
	}

	return {m+1, column};
}
