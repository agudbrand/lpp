--
-- This can only be done once, so I've moved it out of the metaprogram file.
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

  typedef struct Metaprogram Metaprogram;
  typedef struct Cursor Cursor;
  typedef struct SectionNode SectionNode;

  void metaprogramAddMacroSection(Metaprogram* ctx, str indentation, u64 start, u64 macro_idx);
  void metaprogramAddDocumentSection(Metaprogram* ctx, u64 start, str s);

  Cursor* metaprogramNewCursorAfterSection(Metaprogram* ctx);
  void metaprogramDeleteCursor(Metaprogram* ctx, Cursor* cursor);

  str metaprogramGetMacroIndent(Metaprogram* ctx);

  b8  cursorNextChar(Cursor* cursor);
  b8  cursorNextSection(Cursor* cursor);
  u32 cursorCurrentCodepoint(Cursor* cursor);
  b8  cursorInsertString(Cursor* cursor, str text);
  str cursorGetRestOfSection(Cursor* cursor);
  SectionNode* cursorGetSection(Cursor* cursor);

  SectionNode* metaprogramGetCurrentSection(Metaprogram* mp);
  SectionNode* metaprogramGetNextSection(Metaprogram* ctx);

  SectionNode* sectionNext(SectionNode* section);
  SectionNode* sectionPrev(SectionNode* section);

  b8 sectionIsMacro(SectionNode* section);
  b8 sectionIsDocument(SectionNode* section);

  b8 sectionInsertString(SectionNode* section, u64 offset, str s);

  str sectionGetString(SectionNode* section);

  b8 sectionConsumeFromBeginning(SectionNode* section, u64 len);

  str metaprogramGetOutputSoFar(Metaprogram* ctx);
  void metaprogramTrackExpansion(Metaprogram* ctx, u64 from, u64 to);
  s32 metaprogramMapMetaprogramLineToInputLine(Metaprogram* mp, s32 line);
  str metaprogramConsumeCurrentScopeString(Metaprogram* mp);
]]

