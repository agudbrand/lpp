/*
 *  The environment in which a metaprogram is processed into 
 *  its final output.
 */

#ifndef _lpp_metaprogram_h
#define _lpp_metaprogram_h

#include "iro/common.h"
#include "iro/containers/array.h"
#include "iro/containers/pool.h"
#include "iro/containers/linked_pool.h"
#include "iro/containers/list.h"

#include "source.h"

struct MetaprogramContext;
struct Lpp;

using namespace iro;

struct Section;
struct Cursor;
struct Expansion;

typedef SLinkedPool<io::Memory> BufferPool;

/* ================================================================================================ Section
 */
typedef DLinkedPool<Section> SectionPool;
typedef SectionPool::List SectionList;
typedef SectionPool::Node SectionNode;

struct Section
{
  enum class Kind : u8
  {
    Invalid,
    Document,
    Macro,
  };

  Kind kind = Kind::Invalid;

  u64 start_offset = -1;

  io::Memory mem = {};

  SectionNode* node = nullptr;

  u64 macro_idx = -1;
  str macro_indent = nil;


  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */


  b8 initDocument(u64 start_offset, str raw, SectionNode* node);
  b8 initMacro(u64 start_offset, str macro_indent, u64 macro_idx, SectionNode* node);
  void deinit();

  b8 insertString(u64 start, str s);
  b8 consumeFromBeginning(u64 len);
};


namespace iro::io
{

static s64 format(io::IO* io, Section::Kind& kind)
{
  switch (kind)
  {
#define c(x) case Section::Kind::x: return format(io, STRINGIZE(x));
  c(Invalid);
  c(Document);
  c(Macro);
#undef c
    default: return format(io, "INVALID SECTION KIND");
  }
}

}

/* ================================================================================================ Cursor
 */
typedef Pool<Cursor> CursorPool;

struct Cursor
{
  SectionNode* creator = nullptr;
  SectionNode* section = nullptr;

  // the codepoint at the beginning of 'range'
  utf8::Codepoint current_codepoint;
  str range;

  u64 offset; // into current section
};

/* ================================================================================================ Expansion
 */
typedef DLinkedPool<Expansion> ExpansionList;

struct Expansion
{
  u64 from;
  u64 to;
};

/* ================================================================================================
 *  A list of sections and the buffer they are joined into.
 *  We also store information about how the scope started. There is always the global level scope,
 *  then each macro invocation starts its own scope. 
 */
struct Scope
{
  Scope* prev;
  SectionList sections;
  io::Memory* buffer;

  // The macro section where this scope was created.
  // Null when this is the global scope.
  Section* macro_invocation;

  // NOTE(sushi) its assumed the passed buffer is open already.
  b8   init(Scope* prev, io::Memory* buffer, Section* macro_invocation);
  void deinit();

  void writeBuffer(Bytes bytes) { buffer->write(bytes); }

};
typedef SLinkedPool<Scope> ScopePool;

/* ================================================================================================
 */
struct Metaprogram
{
  Lpp* lpp;

  BufferPool buffers;

  SectionPool   sections;
  CursorPool    cursors;
  ExpansionList expansions;

  ScopePool scope_stack;

  io::IO* instream;
  Source* input;
  Source* output;

  SectionNode* current_section;

  // A mapping from a line in the generated metaprogram 
  // to the corresponding line in the original input. 
  // This lets us display the proper location of an error
  // in the input file when something goes wrong in lua.
  // TODO(sushi) the way this is generated is extremely inefficient
  //             and might be a big performance hog when lpp starts
  //             processing larger/more files. A lot of the work done 
  //             in the parser to get the initial byte offset mappings 
  //             that span multiple lines should be moved to the Lexer 
  //             if possible. The generation of this mapping is also 
  //             pretty bad and could probably be lazy so that it only 
  //             happens *if* there's an error to begin with.
  struct InputLineMapping { s32 metaprogram; s32 input; };
  Array<InputLineMapping> input_line_map;


  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */


  b8   init(Lpp* lpp, io::IO* instream, Source* input, Source* output);
  void deinit();

  b8 run();

  Scope* pushScope();
  void   popScope();
  Scope* getCurrentScope();

  void addDocumentSection(u64 start, str s);
  void addMacroSection(s64 start, str indent, u64 macro_idx);

  b8 processScopeSections(Scope* scope);

  str consumeCurrentScope();

  s32 mapMetaprogramLineToInputLine(s32 line);

  template<typename... T>
  b8 errorAt(s32 loc, T... args);

private: 

  // Indexes on the lua stack where important stuff is.
  // Eventually if lpp is ever 'stable' this should be 
  // removed in favor of using const vars set to where we know 
  // these things are on the stack, jsut doing this atm as it 
  // makes working with lua's stack easier in code that changes 
  // often.
  struct 
  {
    s32 errhandler;
    s32 metaprogram;
    s32 metaenv;
    s32 lpp;
    s32 prev_context;
    s32 metaenv_table;
    s32 macro_table;
    s32 MacroExpansion_isTypeOf;
  } I;
};

extern "C"
{

  // TODO(sushi) maybe someday put the lua api here.
  //             Its really not necessary and is just another place I have 
  //             to update declarations at.

}

#endif // _lpp_metaprogram_h
