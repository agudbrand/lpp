--
-- This can only be done once, so I've moved it out of the metaenvironment file.
--

local ffi = require "ffi"
ffi.cdef 
[[

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
		u64 len;
	} str;

	typedef struct
	{
		const u8* ptr;
		u64 len;
	} Bytes;
	

	str get_token_indentation(void* lpp, s32);

	typedef struct MetaprogramBuffer { void* memhandle; s64 memsize; } MetaprogramBuffer;
	MetaprogramBuffer process_file(void* ctx, str path);
	void get_metaprogram_result(MetaprogramBuffer mpbuf, void* outbuf);

	void* metaprogram_get_source(void* ctx);

	s32 advance_at_offset(void* ctx, s32 offset);
	u32 codepoint_at_offset(void* ctx, s32 offset);

	s64 get_advance(str s);
	u32 get_codepoint(str s);

	typedef struct 
	{
		u64 line;
		u64 column;
	} SourceLoc;

	void* source_create(void* ctx, str name);
	str   source_get_name(void* handle);
	b8    source_write_cache(void* handle, Bytes bytes);
	u64   source_get_cache_len(void* handle);
	b8    source_cache_line_offsets(void* handle);
	str   source_get_str(void* handle, u64 offset, u64 length);
	SourceLoc source_get_loc(void* handle, u64 offset);

	void metaenvironment_add_macro_section(void* handle, u64 start);
	void metaenvironment_add_document_section(void* handle, u64 start, str s);

	void* metaenvironment_new_cursor_after_section(void* ctx);
	void* metaenvironment_delete_cursor(void* ctx, void* cursor);

	b8  metaenvironment_cursor_next_char(void* ctx, void* cursor);
	u32 metaenvironment_cursor_current_codepoint(void* ctx, void* cursor);

	SourceLoc metaenvironment_cursor_source_loc(void* ctx, void* cursor);

	b8 metaenvironment_cursor_insert_string(void* ctx, void* cursor, str text);

	str metaenvironment_cursor_get_rest_of_section(void* ctx, void* cursor);
]]

