#include "metaprogram.h"
#include "lpp.h"

#include "iro/linemap.h"
#include "iro/fs/file.h"

#include "stdlib.h"

namespace lpp
{

static Logger logger = 
  Logger::create("lpp.metaprog"_str, Logger::Verbosity::Notice);

/* ----------------------------------------------------------------------------
 */
b8 Section::initDocument(
    u64 start_offset_, 
    String raw, 
    SectionNode* node_,
    io::Memory* buffer)
{
  assert(buffer);
  this->buffer = buffer;
  node = node_;
  start_offset = start_offset_;
  if (!buffer->open(raw.len))
    return false;
  buffer->write(raw);
  kind = Kind::Document;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Section::initMacro(
    u64 start_offset, 
    String macro_indent, 
    u64 macro_idx, 
    SectionNode* node)
{
  this->start_offset = start_offset;
  this->node = node;
  this->macro_idx = macro_idx;
  this->macro_indent = macro_indent;
  kind = Kind::Macro;
  return true;
}

/* ----------------------------------------------------------------------------
 */
void Section::deinit()
{
  switch (kind)
  {
  case Kind::Invalid:
    return;

  case Kind::Document:
    buffer = nullptr;
    break;
  }

  node = nullptr;
  kind = Kind::Invalid;
}

/* ----------------------------------------------------------------------------
 */
b8 Section::insertString(u64 offset, String s)
{
  assert(offset <= buffer->len);

  buffer->reserve(s.len);

  mem::move(
    buffer->ptr + offset + s.len, 
    buffer->ptr + offset, 
    (buffer->len - offset) + 1);
  mem::copy(buffer->ptr + offset, s.ptr, s.len);

  buffer->commit(s.len);

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Section::consumeFromBeginning(u64 len)
{
  if (len > buffer->len)
    return false;

  if (len == buffer->len)
  {
    buffer->clear();
    return true;
  }

  // idk man find a way to not move shit around later maybe with just a String ? 
  // idk we edit stuff too much and IDRC at the moment!!!
  mem::move(buffer->ptr, buffer->ptr + len, buffer->len - len);
  buffer->len -= len;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Metaprogram::init(
    Lpp* lpp, 
    io::IO* instream, 
    Source* input, 
    Source* output,
    Metaprogram* prev)
{
  this->lpp = lpp;
  this->instream = instream;
  this->input = input;
  this->output = output;
  this->prev = prev;
  current_section = nullptr;
  if (!input_line_map.init()) return false;
  if (!buffers.init()) return false;
  if (!sections.init()) return false;
  if (!scope_stack.init()) return false;
  if (!expansions.init()) return false;
  if (!cursors.init()) return false;
  return true;
}

/* ----------------------------------------------------------------------------
 */
void Metaprogram::deinit()
{
  input_line_map.destroy();
  for (auto& section : sections)
    section.deinit();
  sections.deinit();
  for (auto& scope : scope_stack)
    scope.deinit();
  scope_stack.deinit();
  cursors.deinit();
  expansions.deinit();
  for (auto& buffer : buffers)
    buffer.close();
  buffers.deinit();
  *this = {};
}

/* ----------------------------------------------------------------------------
 */
b8 Scope::init(Scope* prev, io::Memory* buffer, Section* macro_invocation)
{
  this->prev = prev;
  this->buffer = buffer;
  this->macro_invocation = macro_invocation;
  sections = SectionList::create();
  return true;
}

/* ----------------------------------------------------------------------------
 */
void Scope::deinit()
{
  sections.deinit();
  *this = {};
}

/* ----------------------------------------------------------------------------
 */
Scope* Metaprogram::pushScope()
{
  auto current = scope_stack.headNode();
  auto node = scope_stack.push();
  auto scope = node->data;

  auto buffer = buffers.push()->data;
  if (!buffer->open())
    return nullptr;
  
  Section* macro_invocation = nullptr;
  if (current_section && current_section->data->kind == Section::Kind::Macro)
    macro_invocation = current_section->data;

  if (!scope->init(
      (current? current->data : nullptr), 
      buffer, 
      macro_invocation))
    return nullptr;

  return scope;
}

/* ----------------------------------------------------------------------------
 */
void Metaprogram::popScope()
{
  auto scope = scope_stack.pop();
  scope.deinit();
}

/* ----------------------------------------------------------------------------
 */
Scope* Metaprogram::getCurrentScope()
{
  return (scope_stack.isEmpty()? nullptr : &scope_stack.head());
}

/* ----------------------------------------------------------------------------
 */
void Metaprogram::addDocumentSection(u64 start, String raw)
{
  auto node = getCurrentScope()->sections.pushTail();
  node->data = sections.pushTail()->data;
  node->data->initDocument(start, raw, node, buffers.push()->data);
}

/* ----------------------------------------------------------------------------
 */
void Metaprogram::addMacroSection(s64 start, String indent, u64 macro_idx)
{
  auto node = getCurrentScope()->sections.pushTail();
  node->data = sections.pushTail()->data;
  node->data->initMacro(start, indent, macro_idx, node);
}

/* ----------------------------------------------------------------------------
 */
static void printExpansion(Source* input, Source* output, Expansion& expansion)
{
  input->cacheLineOffsets();
  output->cacheLineOffsets();

  Source::Loc old = input->getLoc(expansion.from);
  Source::Loc nu  = output->getLoc(expansion.to);
  INFO(
     old.line, ":", old.column, "(", expansion.from, ") -> ", 
     nu.line, ":", nu.column, "(", expansion.to, ")\n");
}

/* ----------------------------------------------------------------------------
 *  Translates a lua error string to the original location in the input.
 *  This is probably only going to be used for syntax errors that occur
 *  when loading the metaprogram into lua.
 *
 *  This doesn't handle all line locations. Unfortunately in a case like 
 *  missing an 'end' lua will report the start of the end in the message.
 *  If we want to handle a case like this we would need to modify the 
 *  message or make a new one which I don't want to get into rn.
 *
 *  TODO(sushi) handle this better.
 *  TODO(sushi) SmallArray might be useful here.
 */
struct TranslatedError
{
  String filename;
  u64 line;
  String error;
};

TranslatedError translateLuaError(Metaprogram& mp, String error)
{
  TranslatedError te = {nil};

  if (error.startsWith("[string"_str))
  {
    error = error.sub("[string "_str.len + 1);
    te.filename = error.sub(0, error.findFirst('"'));
    error = error.sub(te.filename.len + 3);
    String num = error.sub(0, error.findFirst(':'));
    u8 terminated[24];
    if (!num.nullTerminate(terminated, 24))
    {
      ERROR(
        "unable to null terminate lua error line number, buffer too small");
      return {};
    }
    te.line = strtoull((char*)terminated, nullptr, 10);
    te.line = mp.mapMetaprogramLineToInputLine(te.line);
    te.error = error.sub(num.len + 2);
  }
  else
  {
    ERROR("unhandled lua error format: \n", error, "\n");
    return {};
  }

  return te;
}

/* ----------------------------------------------------------------------------
 */
b8 Metaprogram::run()
{
  using enum Section::Kind;

  LuaState& lua = lpp->lua;

  const s32 bottom = lua.gettop();
  defer { lua.settop(bottom); };

  if (!lua.require("errhandler"_str))
  {
    ERROR("failed to load errhandler module");
    return false;
  }
  I.errhandler = lua.gettop();

  // The parsed program, to be executed in phase 2.
  // TODO(sushi) get this to incrementally stream from the parser 
  //             directly to lua.load(). My whole IO setup is too dumb 
  //             to do that atm I think.
  io::Memory parsed_program;
  if (!parsed_program.open())
    return false;
  defer { parsed_program.close(); };

  // ~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~ Phase 1
  
  DEBUG("phase 1\n");

  // Parse into the lua 'metacode' that we run in Phase 2 to form the sections 
  // processed by Phase 3.

  Parser parser;
  if (!parser.init(input, instream, &parsed_program))
    return false;
  defer { parser.deinit(); };

  if (setjmp(parser.err_handler))
    return false;

  if (!parser.run())
    return false;

  if (lpp->print_meta)
    NOTICE(parsed_program.asStr(), "\n");

  // NOTE(sushi) we only want to output the main lpp file's metafile
  if (lpp->output_metafile && prev == nullptr)
  {
    auto file = 
      fs::File::from(lpp->metafile_output, 
          fs::OpenFlag::Write 
        | fs::OpenFlag::Create
        | fs::OpenFlag::Truncate);
    if (isnil(file))
      return FATAL(
          "could not open file at path '", lpp->metafile_output, " for "
          "metafile\n");
    file.write(parsed_program.asStr());
    file.close();
  }

  // TODO(sushi) do this better okay? ^_^
  //
  // Cache off a mapping from lines in the generated file to those in 
  // the original input.
  Source temp;
  temp.init("temp"_str);
  defer { temp.deinit(); };
  temp.writeCache(parsed_program.asStr());
  temp.cacheLineOffsets();
  for (auto& exp : parser.locmap)
  { 
    Source::Loc to = input->getLoc(exp.to);
    Source::Loc from = temp.getLoc(exp.from);
    input_line_map.push(
      {
        .metaprogram = s32(from.line),
        .input = s32(to.line)
      });
  }
  // push a eof line
  input_line_map.push(
      { 
        .metaprogram = input_line_map.last()->metaprogram+1,
        .input = input_line_map.last()->input+1
      });

  // ~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~ Phase 2
  
  DEBUG("phase 2\n");

  // Execute the lua metacode to form the sections we process in the following 
  // phase.

  TRACE("creating global scope\n");
  pushScope();
  defer { popScope(); };

  TRACE("loading parsed program\n");
  if (!lua.loadbuffer(parsed_program.asStr(), (char*)input->name.ptr))
  {
    ERROR(parsed_program.asStr(), "\n");
    auto te = translateLuaError(*this, lua.tostring());
    ERROR(
        "failed to load metaprogram into lua\n",
        te.filename, ":", te.line, ": ", te.error, "\n");
    return false;
  }
  I.metaprogram = lua.gettop();

  // Get the lpp module and set the proper metaprogram context. 
  TRACE("setting lpp module context and metaenv\n");
  if (!lua.require("lpp"_str))
  {
    ERROR("failed to get lpp module for setting metaprogram context\n");
    return false;
  }
  I.lpp = lua.gettop();

  // Save the old context.
  lua.pushstring("context"_str);
  lua.gettable(I.lpp);
  I.prev_context = lua.gettop();

  // Set the new context.
  lua.pushstring("context"_str);
  lua.pushlightuserdata(this);
  lua.settable(I.lpp);

  // Ensure the context is restored.
  defer 
  {
    lua.pushstring("context"_str);
    lua.pushvalue(I.prev_context);
    lua.settable(I.lpp);
  };

  // Save the old metaenv.
  lua.pushstring("metaenv"_str);
  lua.gettable(I.lpp);
  I.prev_metaenv = lua.gettop();

  // Ensure the metaenv is restored.
  defer
  {
    lua.pushstring("metaenv"_str);
    lua.pushvalue(I.prev_metaenv);
    lua.settable(I.lpp);
  };

  // Get the metaenvironment construction function and call it to 
  // make a new env.
  TRACE("constructing metaenvironment\n");
  if (!lua.require("metaenv"_str))
  {
    ERROR("failed to load metaenvironment module\n");
    return false;
  }

  // Call the construction function with this Metaprogram.
  lua.pushlightuserdata(this);
  if (!lua.pcall(1, 1))
  {
    ERROR("metaenvironment construction function failed\n",
          lua.tostring(), "\n");
    return false;
  }
  I.metaenv = lua.gettop();

  // Get the ACTUAL metaenvironment table.
  lua.pushstring("__metaenv"_str);
  lua.gettable(I.metaenv);
  I.metaenv__metaenv = lua.gettop();

  // Get and set the new metaenv on lpp.
  lua.pushstring("metaenv"_str);
  lua.pushvalue(I.metaenv__metaenv);
  lua.settable(I.lpp);

  defer
  {
    // Mark the metaenvironment as having exited when we're done.
    // This helps prevent using metaenvironments from other lpp files as this 
    // is not currently supported. This can occur when two lpp files 
    // require the same lua module and one patches in a function that 
    // uses doc, val, or macro functions in its metaprogram then the other
    // calls that function. The metaenvironment/program at this point is
    // no longer valid and stuff like erroring and placing sections breaks.
    // In order to support this we would probably have to move back to 
    // using a single metaenvironment again which introduces some 
    // complications. Each metaenvironment stores a reference to the 
    // metaprogram that created it and so when one of its doc, val, or macro
    // functions is called in this way it will attempt to perform the logic
    // on the metaprogram that no longer exists. This can be fixed in some 
    // way but I don't feel a need to yet.
    // This might also break if the metaenv is garbage collected by lua.
    // Idk yet if it will be but we'll see :3.
    // Ideally this SHOULD be supported but I'm putting it off for now 
    // until I have an actual use case for it. The (currently apparent)
    // difficulty in getting this to happen also makes it not critical 
    // to implement right away.
    // When we do, the metaenv.lua functions should be designed to use whatever
    // metaprogram is active (I hope its that simple).
    lua.pushstring("exited"_str);
    lua.pushboolean(true);
    lua.settable(I.metaenv__metaenv);
  };

  // NOTE(sushi) I used to do a thing here where the global environment of the 
  //             previous metaenvironment (if any, as in the case of processing 
  //             an lpp file from within another one) was copied into the one 
  //             were about to execute. This was done so that globals would be 
  //             carried through so that we can emulate C defines. I've decided
  //             to not do this anymore as its not very intuitive in the lua 
  //             sense and comes with some silly problems. If it seems to be 
  //             too much of a bother then maybe I'll return to it another day.
  //             If I do, it should probably be implemented as an option
  //             of lpp.processFile().

  // Finally set the metaenvironment of the metacode and call it.
  TRACE("setting environment of metaprogram\n");
  lua.pushvalue(I.metaenv);
  if (!lua.setfenv(I.metaprogram))
  {
    ERROR("failed to set environment of metaprogram\n");
    return false;
  }

  TRACE("executing metaprogram\n");
  lua.pushvalue(I.metaprogram);
  if (!lua.pcall(0,0,I.errhandler))
  {
    ERROR("failed to execute generated metaprogram\n");
    return false;
  }

  // ~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~ Phase 3
  
  DEBUG("phase 3\n");

  // Process each section generated by Phase 2, joining each Document section 
  // into a single buffer and performing macro expansions.
  
  // Retrieve the document callback invoker.
  TRACE("getting 'lpp.runDocumentSectionCallbacks'\n");
  lua.pushstring("runDocumentSectionCallbacks"_str);
  lua.gettable(I.lpp);
  I.lpp_runDocumentSectionCallbacks = lua.gettop();

  TRACE("getting 'lpp.runFinalCallbacks'\n");
  lua.pushstring("runFinalCallbacks"_str);
  lua.gettable(I.lpp);
  I.lpp_runFinalCallbacks = lua.gettop();

  // Retrieve the __metaenv table.
  TRACE("getting '__metaenv'\n");
  lua.pushstring("__metaenv"_str);
  lua.gettable(I.metaenv);
  I.metaenv_table = lua.gettop();

  // Get the metaenv's macro table.
  TRACE("getting metaenvironment's macro invokers list\n");
  lua.pushstring("macro_invokers"_str);
  lua.gettable(I.metaenv_table);
  I.macro_invokers = lua.gettop();

  // Get the metaenv's macro invocation list.
  TRACE("getting metaenvironment's macro invocation list\n");
  lua.pushstring("macro_invocations"_str);
  lua.gettable(I.metaenv_table);
  I.macro_invocations = lua.gettop();

  lua.pushstring("macro_names"_str);
  lua.gettable(I.metaenv_table);
  I.macro_names = lua.gettop();

  TRACE("processing global scope\n");

  if (!processScopeSections(getCurrentScope()))
    return false;

  output->writeCache(getCurrentScope()->buffer->asStr());

  // Process the final callbacks.
  lua.pushvalue(I.lpp_runFinalCallbacks);
  if (!lua.pcall(0, 0, I.errhandler))
    return false;

  output->cacheLineOffsets();

  for (auto& expansion : expansions)
  {
    printExpansion(input, output, expansion);
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Metaprogram::processScopeSections(Scope* scope)
{
  using enum Section::Kind;

  LuaState& lua = lpp->lua;

  // Loop over each section, joining document sections and expanded macros.

  TRACE("processing scope sections\n");

  u64 section_idx = 0;
  for (current_section = scope->sections.head;
       current_section;
       current_section = current_section->next)
  {
    Section* section = current_section->data;
    
    // Any section is always an expansion into the resulting buffer.
    expansions.pushTail({section->start_offset, output->cache.len});

    switch (section->kind)
    {
    case Document:
      {
        TRACE("in ", input->name, ": ");
        TRACE("found Document section\n");

        lua.pushvalue(I.lpp_runDocumentSectionCallbacks);
        if (!lua.pcall(0, 0, I.errhandler))
          return false;

        // Simple write to the output cache.
        scope->writeBuffer(section->buffer->asStr());
      }  
      break;

    case Macro:
      {
        TRACE("in ", input->name, ": ");
        TRACE("found Macro section\n");
        SectionList::Node* save = current_section;
        defer { current_section = save; };

        // Create a new Scope for the macro.
        // TODO(sushi) this prob doesnt need to be heap.
        Scope* macro_scope = pushScope();
        if (!macro_scope)
          return false;
        defer { popScope(); };

        // Get the macro and execute it.
        lua.pushinteger(section->macro_idx);
        lua.gettable(I.macro_invokers);
        const s32 I_macro = lua.gettop();

        TRACE("invoking macro\n");
        if (!lua.pcall(0, 1, I.errhandler))
          return false;
        const s32 I_macro_result = lua.gettop();

        // Process the sections produced within the macro, if any.
        if (!processScopeSections(macro_scope))
          return false;

        // Append the result of the scope.
        scope->writeBuffer(macro_scope->buffer->asStr());

        if (!lua.isnil())
        {
          // Error handling and MacroExpansion stuff is handled
          // in lua, we know this is a string if there's anything.
          scope->writeBuffer(lua.tostring());
        }

        lua.pop();
      }
      break;
    }
  }
  return true;
}

/* ----------------------------------------------------------------------------
 */
String Metaprogram::consumeCurrentScope()
{
  auto scope = getCurrentScope();
  if (!processScopeSections(scope))
    return nil;

  // TODO(sushi) properly allocate this in a way that it can be cleaned up
  //             do not feel like setting that up right now.
  String out = scope->buffer->asStr().allocateCopy();
  scope->buffer->clear();

  while (scope->sections.head)
  {
    scope->sections.head->data->deinit();
    scope->sections.remove(scope->sections.head);
  }
  return out;
}

/* ----------------------------------------------------------------------------
 */
s32 Metaprogram::mapMetaprogramLineToInputLine(s32 line)
{
  for (auto& mapping : input_line_map)
  {
    if (line <= mapping.metaprogram)
      return mapping.input;
  }
  return -1;
}

/* ----------------------------------------------------------------------------
 */
template<typename... T>
b8 Metaprogram::errorAt(s32 offset, T... args)
{
  input->cacheLineOffsets();
  Scope* scope = getCurrentScope();
  while (scope->prev != nullptr)
  {
    Source::Loc loc = input->getLoc(scope->macro_invocation->start_offset);
    INFO(input->name, ":", loc.line, ":", loc.column, 
         ": in scope invoked here:\n");
    scope = scope->prev;
  }

  Source::Loc loc = input->getLoc(offset);
  ERROR(input->name, ":", loc.line, ":", loc.column, ": ", args..., "\n");
  return false;
}

}

using namespace lpp;

extern "C"
{
/* IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT !!!!!!
 *
 *
 *
 * IF you CHANGE or ADD STRUCTURES here you MUST make sure that the proper 
 * stuff is UPDATED in cdefs.lua!!!! Giving luajit an incorrect structure def
 * is a REALLY good way to cause runtime errors that are HARD, TOUGH, and 
 * might I even say DIFFICULT to figure out!
 *
 *
 *
 * IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT !!!!!!
 */

// Forward decls, as needed.
b8 sectionIsDocument(SectionNode* section);
b8 sectionIsMacro(SectionNode* section);

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
void metaprogramAddMacroSection(
    Metaprogram* mp, 
    String indent, 
    u64 start, 
    u64 macro_idx)
{
  mp->addMacroSection(start, indent, macro_idx);
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
void metaprogramAddDocumentSection(Metaprogram* mp, u64 start, String raw)
{
  mp->addDocumentSection(start, raw);
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
Cursor* metaprogramNewCursorAfterSection(Metaprogram* mp)
{
  if (!mp->current_section->next)
    return nullptr;

  Cursor* cursor = mp->cursors.add();
  cursor->creator = mp->current_section->next;
  cursor->section = cursor->creator;
  cursor->range = cursor->section->data->buffer->asStr();
  cursor->current_codepoint = 
    utf8::decodeCharacter(cursor->range.ptr, cursor->range.len);
  return cursor;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
void metaprogramDeleteCursor(Metaprogram* mp, Cursor* cursor)
{
  mp->cursors.remove(cursor);
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
String metaprogramGetMacroIndent(Metaprogram* mp)
{
  assert(sectionIsMacro(mp->current_section));
  return mp->current_section->data->macro_indent;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
String metaprogramGetOutputSoFar(Metaprogram* mp)
{
  return mp->output->cache.asStr();
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
void metaprogramTrackExpansion(Metaprogram* mp, u64 from, u64 to)
{
  mp->expansions.pushTail({from, to});
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
s32 metaprogramMapMetaprogramLineToInputLine(Metaprogram* mp, s32 line)
{
  return mp->mapMetaprogramLineToInputLine(line);
}

/* ----------------------------------------------------------------------------
 */
struct LineAndColumn
{
  u64 line;
  u64 column;
};

LPP_LUAJIT_FFI_FUNC
LineAndColumn metaprogramMapInputOffsetToLineAndColumn(
    Metaprogram* mp, 
    u64 offset)
{
  assert(mp);
  assert(offset < mp->input->cache.len);
  LineAndColumn lac;
  Source::Loc loc = mp->input->getLoc(offset);
  lac.line = loc.line;
  lac.column = loc.column;
  return lac;
}

/* ----------------------------------------------------------------------------
 *  Processes the sections of the current scope and returns the result as a 
 *  String. This does not end the scope.
 *
 *  TODO(sushi) a version of this could be made that forks the scope and 
 *              returns it to be evaluated later.
 */
LPP_LUAJIT_FFI_FUNC
String metaprogramConsumeCurrentScopeString(Metaprogram* mp)
{
  return mp->consumeCurrentScope();
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* metaprogramGetTopMacroInvocation(Metaprogram* mp)
{
  assert(sectionIsMacro(mp->current_section));

  SectionNode* walk = mp->current_section;

  while (sectionIsMacro(walk) && walk->prev)
    walk = walk->prev;

  return walk;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
Metaprogram* metaprogramGetPrev(Metaprogram* mp)
{
  assert(mp);
  return mp->prev; 
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
Scope* metaprogramGetCurrentScope(Metaprogram* mp)
{
  assert(mp);
  return mp->getCurrentScope();
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
Scope* scopeGetPrev(Scope* scope)
{
  assert(scope);
  return scope->prev;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
u64 scopeGetInvokingMacroIdx(Scope* scope)
{
  assert(scope);
  return (scope->macro_invocation? scope->macro_invocation->macro_idx : 0);
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
b8 cursorNextChar(Cursor* cursor)
{
  Section* s = cursor->section->data;

  if (s->kind == Section::Kind::Macro)
    return false;

  if (cursor->range.len == 0)
    return false;

  cursor->range.increment(cursor->current_codepoint.advance);
  if (cursor->range.len == 0)
    return false;

  cursor->current_codepoint = 
    utf8::decodeCharacter(cursor->range.ptr, cursor->range.len);
  return true;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
u32 cursorCurrentCodepoint(Cursor* cursor)
{
  return cursor->current_codepoint.codepoint;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
b8 cursorInsertString(Cursor* cursor, String text)
{
  Section* s = cursor->section->data;
  if (s->kind != Section::Kind::Document)
    return false;
  
  assert(cursor->range.ptr >= s->buffer->ptr);

  u64 cursor_offset = cursor->range.ptr - s->buffer->ptr;

  if (!s->insertString(cursor_offset, text))
    return false;

  u8* new_pos = s->buffer->ptr + cursor_offset + text.len;
  u64 new_len = (s->buffer->ptr + s->buffer->len) - new_pos;
  cursor->range = {new_pos, new_len};

  return true;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* cursorGetSection(Cursor* cursor)
{
  return cursor->section;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
String cursorGetRestOfSection(Cursor* cursor)
{
  assert(sectionIsDocument(cursorGetSection(cursor)));
  return cursor->range;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* metaprogramGetNextSection(Metaprogram* mp)
{
  return mp->current_section->next;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* metaprogramGetCurrentSection(Metaprogram* mp)
{
  return mp->current_section;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
String sectionGetString(SectionNode* section)
{
  return section->data->buffer->asStr();
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* sectionNext(SectionNode* section)
{
  return section->next;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* sectionPrev(SectionNode* section)
{
  return section->prev;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
b8 sectionIsMacro(SectionNode* section)
{
  return section->data->kind == Section::Kind::Macro;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
b8 sectionIsDocument(SectionNode* section)
{
  return section->data->kind == Section::Kind::Document;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
b8 sectionInsertString(SectionNode* section, u64 offset, String s)
{
  assert(sectionIsDocument(section));
  return section->data->insertString(offset, s);
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
b8 sectionAppendString(SectionNode* section, String s)
{
  assert(sectionIsDocument(section));
  return section->data->insertString(section->data->buffer->len, s);
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
b8 sectionConsumeFromBeginning(SectionNode* section, u64 len)
{
  assert(sectionIsDocument(section));
  return section->data->consumeFromBeginning(len);
}

/* ----------------------------------------------------------------------------
 *  Get the offset into the input file where this section starts.
 */
LPP_LUAJIT_FFI_FUNC
u64 sectionGetStartOffset(SectionNode* section)
{
  assert(section);
  return section->data->start_offset;
}

}

