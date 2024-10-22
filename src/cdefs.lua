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

  typedef struct String
  {
    const u8* s;
    u64 len;
  } String;

  typedef struct
  {
    const u8* ptr;
    u64 len;
  } Bytes;
  
  String getTokenIndentation(void* lpp, s32);

  typedef struct MetaprogramBuffer { void* memhandle; s64 memsize; } MetaprogramBuffer;
  MetaprogramBuffer processFile(void* ctx, String path);
  void getMetaprogramResult(MetaprogramBuffer mpbuf, void* outbuf);

  typedef struct Metaprogram Metaprogram;
  typedef struct Cursor Cursor;
  typedef struct SectionNode SectionNode;
  typedef struct Scope Scope;

  void metaprogramAddMacroSection(
    Metaprogram* ctx, 
    String indentation, 
    u64 start, 
    u64 macro_idx);
  void metaprogramAddDocumentSection(Metaprogram* ctx, u64 start, String s);

  Cursor* metaprogramNewCursorAfterSection(Metaprogram* ctx);
  void metaprogramDeleteCursor(Metaprogram* ctx, Cursor* cursor);

  String metaprogramGetMacroIndent(Metaprogram* ctx);

  b8  cursorNextChar(Cursor* cursor);
  b8  cursorNextSection(Cursor* cursor);
  u32 cursorCurrentCodepoint(Cursor* cursor);
  b8  cursorInsertString(Cursor* cursor, String text);
  String cursorGetRestOfSection(Cursor* cursor);
  SectionNode* cursorGetSection(Cursor* cursor);

  SectionNode* metaprogramGetCurrentSection(Metaprogram* mp);
  SectionNode* metaprogramGetNextSection(Metaprogram* ctx);

  SectionNode* sectionNext(SectionNode* section);
  SectionNode* sectionPrev(SectionNode* section);

  b8 sectionIsMacro(SectionNode* section);
  b8 sectionIsDocument(SectionNode* section);

  b8 sectionInsertString(SectionNode* section, u64 offset, String s);
  b8 sectionAppendString(SectionNode* section, String s);

  String sectionGetString(SectionNode* section);

  b8 sectionConsumeFromBeginning(SectionNode* section, u64 len);

  String metaprogramGetOutputSoFar(Metaprogram* ctx);
  void metaprogramTrackExpansion(Metaprogram* ctx, u64 from, u64 to);
  s32 metaprogramMapMetaprogramLineToInputLine(Metaprogram* mp, s32 line);
  String metaprogramConsumeCurrentScopeString(Metaprogram* mp);

  SectionNode* metaprogramGetTopMacroInvocation(Metaprogram* mp);
  Metaprogram* metaprogramGetPrev(Metaprogram* mp);

  Scope* metaprogramGetCurrentScope(Metaprogram* mp);
  Scope* scopeGetPrev(Scope* scope);
  u64 scopeGetInvokingMacroIdx(Scope* scope);

  u64 sectionGetStartOffset(SectionNode* section);

  typedef struct
  {
    u64 line;
    u64 column;
  } LineAndColumn;

  LineAndColumn metaprogramMapInputOffsetToLineAndColumn(
    Metaprogram* mp, 
    u64 offset);
]]

