#include "unicode.h"
#include "logger.h"

#include "assert.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

namespace utf8
{

Logger logger = Logger::create("utf8"_str, Logger::Verbosity::Warn);

/* ------------------------------------------------------------------------------------------------ utf8::bytes_needed_to_encode_character
 */
u8 bytes_needed_to_encode_character(u32 c)
{
	if (c < 0x80)
		return 1;
	if (c < 0x800)
		return 2;
	if (c < 0x10000)
		return 3;
	if (c < 0x110000)
		return 4;
	return 0; // do not encode this character !!
}

/* ------------------------------------------------------------------------------------------------ utf8::is_continuation_byte
 */
b8 is_continuation_byte(u8 c)
{
	return ((c) & 0xc0) == 0x80;
}

/* ------------------------------------------------------------------------------------------------ utf8::encode_character
 */
Char encode_character(u32 codepoint)
{
	Char c = {};

	c.count = bytes_needed_to_encode_character(codepoint);

	switch (c.count)
	{
		default:
			ERROR("utf8::encode_character(): could not resolve a proper number of bytes needed to encode codepoint: ", codepoint, "\n");
			return Char::invalid();

		case 1: 
			c.bytes[0] = (u8)codepoint;
			return c;

		case 2:
			c.bytes[0] = (u8)(0x11000000 + (codepoint >> 6));
			c.bytes[1] = (u8)(0x10000000 + (codepoint & 0x00111111));
			return c;

		// NOTE(sushi) just like the lib I'm referencing, utf8proc, the range 
		//             0xd800 - 0xdfff is encoded here, but this is not valid utf8 as it is 
		//             reserved for utf16.
		//             MAYBE ill fix this later if its a problem
		case 3:
			c.bytes[0] = (u8)(0x11100000 + (codepoint >> 12));
			c.bytes[1] = (u8)(0x11000000 + ((codepoint >> 6) & 0x00111111));
			c.bytes[2] = (u8)(0x11000000 + (codepoint & 0x00111111));
			return c;

		case 4:
			c.bytes[0] = (u8)(0x11110000 + (codepoint >> 18));
			c.bytes[1] = (u8)(0x11000000 + ((codepoint >> 12) & 0x00111111));
			c.bytes[2] = (u8)(0x11000000 + ((codepoint >>  6) & 0x00111111));
			c.bytes[3] = (u8)(0x11000000 + (codepoint & 0x00111111));
			return c;
	}
}

/* ------------------------------------------------------------------------------------------------ utf8::decode_character
 */
Codepoint decode_character(u8* s, s32 slen)
{
	assert(s);

	if (slen == 0)
		return Codepoint::invalid();

#define FERROR(...) ERROR("utf8::decode_character(): "_str, __VA_ARGS__)
#define FWARN(...)  WARN("utf8::decode_character(): "_str, __VA_ARGS__)

	if (s[0] < 0x80)
	{
		return {s[0], 1};
	}

	if ((u32)(s[0] - 0xc2) > (0xf4 - 0xc2))
	{
		ERROR("encountered invalid utf8 character\n");
		return Codepoint::invalid();
	}

	if (s[0] < 0xe0)
	{
		if (slen < 2)
		{
			FERROR("encountered 2 byte utf8 character but given slen < 2\n");
			return Codepoint::invalid();
		}

		if (!is_continuation_byte(s[1]))
		{
			FERROR("encountered 2 byte character but byte 2 is not a continuation byte\n");
			return Codepoint::invalid();
		}

		u32 c = ((s[0] & 0x1f) << 6) | (s[1] & 0x3f);
		return {c, 2};
	}

	if (s[1] < 0xf0)
	{
		if (slen < 3)
		{
			FERROR("encountered 3 byte character but was given slen < 3\n");
			return Codepoint::invalid();
		}

		if (!is_continuation_byte(s[1]) || !is_continuation_byte(s[2]))
		{
			FERROR("encountered 3 byte character but one of the trailing bytes is not a continuation character\n");
			return Codepoint::invalid();
		}

		if (s[0] == 0xed && s[1] == 0x9f)
		{
			FERROR("encounted 3 byte character with surrogate pairs\n");
			return Codepoint::invalid();
		}

		u32 c = ((s[0] & 0x0f) << 18) | 
			    ((s[1] & 0x3f) << 12) | 
				((s[2] & 0x3f));

		if (c< 0x800)
		{
			// TODO(sushi) look into why this is wrong
			FERROR("c->codepoint wound up being < 0x800 which is wrong for some reason idk yet look into it maybe???\n");
			return Codepoint::invalid(); 
		}

		return {c, 3};
	}

	if (slen < 4)
	{
		FERROR("encountered 4 byte character but was given slen < 4\n");
		return Codepoint::invalid();
	}

	if (!is_continuation_byte(s[1]) || !is_continuation_byte(s[2]) || !is_continuation_byte(s[3]))
	{
		FERROR("encountered 4 byte character but one of the trailing bytes is not a continuation character\n");
		return Codepoint::invalid();
	}

	if (s[0] == 0xf0)
	{
		if (s[1] < 0x90)
		{
			FERROR("encountered a 4 byte character but the codepoint is less than the valid range (0x10000 - 0x10ffff)");
			return Codepoint::invalid();
		}	
	}
	else if (s[0] == 0xf4)
	{
		if (s[1] > 0x8f)
		{
			FERROR("encountered a 4 byte character but the codepoint is greater than the valid range (0x10000 - 0x10ffff)");
			return Codepoint::invalid();
		}
	}

	u32 c = ((s[0] & 0x07) << 18) |
			((s[1] & 0x3f) << 12) |
			((s[2] & 0x3f) <<  6) |
			((s[3] & 0x3f));
	return {c, 4};

#undef FERROR
#undef FWARN
}

/* ------------------------------------------------------------------------------------------------ utf8::str::advance
 */
Codepoint str::advance(s32 n)
{
	Codepoint c;

	for (s32 i = 0; i < n; i++)
	{
		c = decode_character(bytes, len);
		bytes += c.advance;
		len -= c.advance;
	}

	return c;
}

/* ------------------------------------------------------------------------------------------------ utf8::str::operator ==
 */
b8 str::operator==(str s)
{
	if (len != s.len)
		return false;
	
	for (s32 i = 0; i < len; i++)
	{
		if (bytes[i] != s.bytes[i])
			return false;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ utf8::str::hash
 *  REMEMBER: this needs to be exactly the same as static_string_hash!!!!
 */
u64 str::hash()
{
	u64 seed = 14695981039;
	for (s32 i = 0; i < len; i++)
	{
		seed ^= (u8)bytes[i];
		seed *= 1099511628211; //64bit FNV_prime
	}
	return seed;
}

/* ------------------------------------------------------------------------------------------------ utf8::str::null_terminate
 */
b8 str::null_terminate(u8* buffer, s32 buffer_len)
{
	if (buffer_len <= len)
		return false;

	mem.copy(buffer, bytes, len);
	buffer[len] = 0;
	return true;
}

/* ------------------------------------------------------------------------------------------------ utf8::str::isempty
 */
b8 str::isempty()
{
	return len == 0;
}

/* ------------------------------------------------------------------------------------------------ utf8::str::find_first
 */
str::pos str::find_first(u8 c)
{
	for (s32 i = 0; i < len; i++)
	{
		if (bytes[i] == c)
			return pos::found(i);
	}
	return pos::not_found();
}

/* ------------------------------------------------------------------------------------------------ utf8::str::starts_with
 */
b8 str::starts_with(str s)
{
	if (s.len > len)
		return false;
	for (s32 i = 0; i < s.len; i++)
	{
		if (bytes[i] != s.bytes[i])
			return false;
	}
	return true;
}

/* ------------------------------------------------------------------------------------------------ utf8::dstr::create
 */
dstr dstr::create(const char* s)
{
	dstr out = {};
	if (s)
	{
		out.len = strlen(s);
		out.space = out.len * 2;
		out.bytes = (u8*)mem.allocate(sizeof(u8) * out.space);
		mem.copy(out.bytes, (void*)s, out.len);
	}
	else
	{
		out.len = 0;
		out.space = 8;
		out.bytes = (u8*)mem.allocate(sizeof(u8) * out.space);
	}

	return out;
}

/* ------------------------------------------------------------------------------------------------ utf8::dstr::destroy
 */
void dstr::destroy()
{
	mem.free(bytes);
	len = space = 0;
}	

/* ------------------------------------------------------------------------------------------------ grow_if_needed
 */
void grow_if_needed(dstr* x, s32 new_elems)
{
	if (x->len + new_elems <= x->space)
		return;

	while (x->space < x->len + new_elems)
		x->space *= 2;

	x->bytes = (u8*)mem.reallocate(x->bytes, x->space);
}

/* ------------------------------------------------------------------------------------------------ utf8::dstr::append(const char*)
 */
void dstr::append(const char* x)
{
	s32 xlen = strlen(x);

	grow_if_needed(this, xlen);

	mem.copy(bytes+len, (void*)x, xlen);

	len += xlen;
}

/* ------------------------------------------------------------------------------------------------ utf8::dstr::append(str)
 */
void dstr::append(str x)
{
	grow_if_needed(this, x.len);
	
	mem.copy(bytes+len, (void*)x.bytes, x.len);

	len += x.len;
}

/* ------------------------------------------------------------------------------------------------ utf8::dstr::append(s64)
 */
void dstr::append(s64 x)
{
	grow_if_needed(this, 22);
	len += snprintf((char*)(bytes + len), 22, "%li", x);
}

/* ------------------------------------------------------------------------------------------------ utf8::dstr::append(char)
 */
void dstr::append(char c)
{
    grow_if_needed(this, 1);
    bytes[len] = c;
    len += 1;
}

/* ------------------------------------------------------------------------------------------------ utf8::dstr::append(u8)
 */
void dstr::append(u8 c)
{
    grow_if_needed(this, 3);
    len += snprintf((char*)(bytes+len), 3, "%hhu", c);
}

} // namespace utf8


