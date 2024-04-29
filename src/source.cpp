#include "source.h"
#include "unicode.h"
#include "lpp.h"

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

/* ------------------------------------------------------------------------------------------------ Source::write_cache
 */
b8 Source::write_cache(Bytes slice)
{
	line_offsets_calculated = false;
	cache.write(slice);
	return true;
}

/* ------------------------------------------------------------------------------------------------ Source::cache_line_offsets
 */
b8 Source::cache_line_offsets()
{
	if (line_offsets_calculated)
		return true;

	// TODO(sushi) continue calculated offsets if there are any
	line_offsets.destroy();
	line_offsets = Array<u64>::create();

	line_offsets.push(0);

	str s = cache.as_str();
	for (s64 i = 0; i < s.len; i++)
	{
		if (s.bytes[i] == '\n')
			line_offsets.push(i+1);
	}

	line_offsets_calculated = true;
	return true;
}

/* ------------------------------------------------------------------------------------------------ Source::get_str
 */
str Source::get_str(u64 loc, u64 len)
{
	return {cache.buffer + loc, len};
}

/* ------------------------------------------------------------------------------------------------ Source::get_loc
 */
Source::Loc Source::get_loc(u64 loc)
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
		utf8::Codepoint c = utf8::decode_character(cache.buffer + offset, 4);
		offset += c.advance;
		column += 1;
	}

	return {m+1, column};
}

/* ------------------------------------------------------------------------------------------------ C api
 */
extern "C"
{

/* ------------------------------------------------------------------------------------------------ source_create
 */
Source* source_create(MetaprogramContext* ctx, str name)
{
	Source* source = ctx->lpp->sources.add();
	source->init(name);
	return source;
}

/* ------------------------------------------------------------------------------------------------ source_destroy
 */
void source_destroy(MetaprogramContext* ctx, Source* source)
{
	source->deinit();
	ctx->lpp->sources.remove(source);
}

/* ------------------------------------------------------------------------------------------------ source_get_name
 */
str source_get_name(Source* handle)
{
	return handle->name;
}

/* ------------------------------------------------------------------------------------------------ source_write_cache
 */
b8 source_write_cache(Source* handle, Bytes slice)
{
	return handle->write_cache(slice);
}

/* ------------------------------------------------------------------------------------------------ source_write_cache
 */
u64 source_get_cache_len(Source* handle)
{
	return handle->cache.len;
}

/* ------------------------------------------------------------------------------------------------ source_cache_line_offsets
 */
b8 source_cache_line_offsets(Source* handle)
{
	return handle->cache_line_offsets();
}

/* ------------------------------------------------------------------------------------------------ source_get_str
 */
str source_get_str(Source* handle, u64 offset, u64 length)
{
	return handle->get_str(offset, length);
}

/* ------------------------------------------------------------------------------------------------ source_get_loc
 */
Source::Loc source_get_loc(Source* handle, u64 offset)
{
	return handle->get_loc(offset);
}

}
