--
-- This can only be done once, so I've moved it out of the metaenvironment file.
--

local ffi = require "ffi"
ffi.cdef [[

	typedef uint8_t  u8;
	typedef uint16_t u16;
	typedef uint32_t u32;
	typedef uint64_t u64;
	typedef int8_t   s8;
	typedef int16_t  s16;
	typedef int32_t  s32;
	typedef int64_t  s64;
	typedef float    f32;
	typedef double   f64;
	typedef u8       b8; // booean type

	typedef struct str
	{
		const u8* s;
		s32 len;
	} str;

	str get_token_indentation(void* lpp, s32);

	typedef struct MetaprogramBuffer { void* memhandle; s64 memsize; } MetaprogramBuffer;
	MetaprogramBuffer process_file(void* ctx, str path);
	void get_metaprogram_result(MetaprogramBuffer mpbuf, void* outbuf);

	typedef struct 
	{
		str streamname;
		s32 line;
		s32 column;
	} SourceLocation;

	SourceLocation get_token_source_location(void* ctx, s32 idx);
	

	s32 advance_at_offset(void* ctx, s32 offset);
	u32 codepoint_at_offset(void* ctx, s32 offset);
]]

