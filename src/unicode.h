/*
 *  Stuff related to dealing with unicode.
 */

#ifndef _lpp_unicode_h
#define _lpp_unicode_h

#include "common.h"

namespace utf8
{

/* 
 *  Using https://github.com/JuliaStrings/utf8proc/blob/master/utf8proc.c
 *  as a reference for some of this stuff.
 */

// Returns how many bytes are needed to encode the given 
// codepoint. If 0 is returned then the given code point
// should not be encoded.
u8 bytes_needed_to_encode_character(u32 codepoint);

b8 is_continuation_byte(u8 c);

/* ================================================================================================ utf8::Char
 *  Representation of an encoded character with enough space for a 4 byte character and a count
 *  of how many bytes were used.
 */
struct Char
{
	u8 bytes[4];
	u8 count;

	static Char invalid() { return {{},0}; }
	b8 is_valid() { return count != 0; }
};

// Encodes the given codepoint into 'ch'.
// Returns false if failure occurs for any reason.
Char encode_character(u32 codepoint);

/* ================================================================================================ utf8::Codepoint
 *  Representation of a decoded unicode codepoint along with the number of bytes needed it to
 *  represent it in utf8
 */
struct Codepoint
{
	u32 codepoint;
	u32 advance;

	static Codepoint invalid() { return {(u32)-1,0}; }
	b8 is_valid() { return codepoint != (u32)-1; }

	operator bool() { return is_valid(); }
	operator u32() { return codepoint; }

	b8 operator ==(u32  x) { return codepoint == x; }
	b8 operator !=(u32  x) { return codepoint != x; }
	b8 operator ==(char x) { return codepoint == x; }
	b8 operator !=(char x) { return codepoint != x; }
};

// Attempt to decode the character at 's' into 'codepoint'.
// If decoding fails for any reason, false is returned.
// TODO(sushi) it may be nice to have an 'unsafe' version w all the valid checks disabled 
//             if we're ever confident we're dealing with proper utf8 strings
Codepoint decode_character(u8* s, s32 slen);


/* ================================================================================================ utf8::str
 *  A string encoded in utf8.
 */
struct str
{
	u8* bytes;
	s64 len;

	b8 isempty();

	u64 hash();

	// Advances this str by 'n' codepoints and returns the last codepoint passed.
	// This may return invalid codepoints!
	Codepoint advance(s32 n = 1);

	// If the provided buffer is large enough, copy this 
	// str into it followed by a null terminator and return true.
	// Otherwise return false;
	b8 null_terminate(u8* buffer, s32 buffer_len);

	b8 operator ==(str s);

	u8* begin() { return bytes; }
	u8* end()   { return bytes + len; }

	struct pos
	{
		s64 x;

		static pos found(s64 x) { return {x}; }
		static pos not_found() { return {-1}; }
		b8 found() { return x != -1; }
		operator bool() { return found(); }
		operator s64() { return x; }
	};

	// each find function should provide a byte and codepoint variant
	pos find_first(u8 c);

	b8 starts_with(str s);
};

/* ================================================================================================ utf8::dstr
 *  A utf8 string builder
 */
struct dstr
{
	union 
	{
		struct 
		{
			u8* bytes;
			s64 len;
		};
		str fin;
	};

	s32 space;

	static dstr create(const char* s = 0);

	void destroy();

	void reset()
	{
		len = 0;
	};

	void append(const char* s);
	void append(str s);
	void append(s64 x);
    void append(char c);
    void append(u8 x);

	u64 hash() { return fin.hash(); }

	template<typename... T>
	void appendv(T... args)
	{
		(append(args), ...); 
	}
};

void print(str s);
void print(dstr s);

} // namespace utf8

// since we only use utf8 internally, just bring the two string types into the global namespace
using str  = utf8::str;
using dstr = utf8::dstr;

static str operator ""_str (const char* s, size_t len)
{
	return str{(u8*)s, (s32)len};
}


#endif // _lpp_unicode_h
