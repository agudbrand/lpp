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
	

	str getTokenIndentation(void* lpp, s32);

	typedef struct MetaprogramBuffer { void* memhandle; s64 memsize; } MetaprogramBuffer;
	MetaprogramBuffer processFile(void* ctx, str path);
	void getMetaprogramResult(MetaprogramBuffer mpbuf, void* outbuf);

	void* metaprogramGetSource(void* ctx);

	s32 advanceAtOffset(void* ctx, s32 offset);
	u32 codepointAtOffset(void* ctx, s32 offset);

	s64 getAdvance(str s);
	u32 getCodepoint(str s);

	typedef struct 
	{
		u64 line;
		u64 column;
	} SourceLoc;

	void* sourceCreate(void* ctx, str name);
	str   sourceGetName(void* handle);
	b8    sourceWriteCache(void* handle, Bytes bytes);
	u64   sourceGetCacheLen(void* handle);
	b8    sourceCacheLineOffsets(void* handle);
	str   sourceGetStr(void* handle, u64 offset, u64 length);
	SourceLoc sourceGetLoc(void* handle, u64 offset);

	void metaenvironmentAddMacroSection(void* handle, u64 start);
	void metaenvironmentAddDocumentSection(void* handle, u64 start, str s);

	void* metaenvironmentNewCursorAfterSection(void* ctx);
	void* metaenvironmentDeleteCursor(void* ctx, void* cursor);

	b8  metaenvironmentCursorNextChar(void* ctx, void* cursor);
	u32 metaenvironmentCursorCurrentCodepoint(void* ctx, void* cursor);

	SourceLoc metaenvironmentCursorSourceLoc(void* ctx, void* cursor);

	b8 metaenvironmentCursorInsertString(void* ctx, void* cursor, str text);

	str metaenvironmentCursorGetRestOfSection(void* ctx, void* cursor);
]]

