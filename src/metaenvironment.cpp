#include "metaenvironment.h"
#include "lpp.h"

#include "iro/fs/file.h"

static Logger logger = Logger::create("metaenv"_str, Logger::Verbosity::Trace);

/* ------------------------------------------------------------------------------------------------ Section::initDocument
 */
b8 Section::initDocument(u64 start_offset_, str raw, SectionNode* node_)
{
	node = node_;
	start_offset = start_offset_;
	if (!mem.open(raw.len))
		return false;
	mem.write(raw);
	kind = Kind::Document;
	return true;
}

/* ------------------------------------------------------------------------------------------------ Section::initMacro
 */
b8 Section::initMacro(u64 start_offset_, str macro_indent_, u64 macro_idx_, SectionNode* node_)
{
	start_offset = start_offset_;
	node = node_;
	macro_idx = macro_idx_;
	macro_indent = macro_indent_;
	kind = Kind::Macro;
	return true;
}

/* ------------------------------------------------------------------------------------------------ Section::deinit
 */
void Section::deinit()
{
	switch (kind)
	{
	case Kind::Invalid:
		return;

	case Kind::Document:
		mem.close();
		break;
	}

	node = nullptr;
}

/* ------------------------------------------------------------------------------------------------ Section::insertString
 */
b8 Section::insertString(u64 offset, str s)
{
	assert(offset <= mem.len);

	mem.reserve(s.len + 1);
	mem.commit(s.len + 1);

	mem::move(mem.buffer + offset + s.len, mem.buffer + offset, mem.len - offset);
	mem::copy(mem.buffer + offset, s.bytes, s.len);

	return true;
}

/* ------------------------------------------------------------------------------------------------ Section::consumeFromBeginning
 */
b8 Section::consumeFromBeginning(u64 len)
{
	if (len > mem.len)
		return false;

	if (len == mem.len)
	{
		mem.clear();
		return true;
	}

	// idk man find a way to not move shit around later maybe with just a str ? idk we edit stuff
	// too much and IDRC at the moment!!!
	mem::move(mem.buffer, mem.buffer + len, mem.len - len);
	mem.len -= len;
	return true;
}

/* ------------------------------------------------------------------------------------------------ Metaenvironment::init
 */
b8 Metaenvironment::init(Lpp* lpp, Source* input, Source* output)
{
	this->lpp = lpp;
	this->input = input;
	this->output = output;
	sections = SectionPool::create();
	section_list = SectionList::create();
	cursors = CursorPool::create();
	expansions.init();
	return true;
}

/* ------------------------------------------------------------------------------------------------ Metaenvironment::deinit
 */
void Metaenvironment::deinit()
{
	sections.destroy();
	cursors.destroy();
	expansions.deinit();
	*this = {};
}

/* ------------------------------------------------------------------------------------------------ Metaenvironment::addDocumentSection
 */
void Metaenvironment::addDocumentSection(u64 start, str raw)
{
	Section* s = sections.add();
	s->initDocument(start, raw, section_list.pushTail(s));
}

/* ------------------------------------------------------------------------------------------------ Metaenvironment::addMacroSection
 */
void Metaenvironment::addMacroSection(s64 start, str indent, u64 macro_idx)
{
	Section* s = sections.add();
	s->initMacro(start, indent, macro_idx, section_list.pushTail(s));
}

static void printExpansion(Source* input, Source* output, Expansion& expansion)
{
	input->cacheLineOffsets();
	output->cacheLineOffsets();

	Source::Loc old = input->getLoc(expansion.from);
	Source::Loc nu  = output->getLoc(expansion.to);
	INFO(old.line, ":", old.column, "(", expansion.from, ") -> ", nu.line, ":", nu.column, "(", expansion.to, ")\n");
}

/* ------------------------------------------------------------------------------------------------ Metaenvironment::processSections
 *  This expects the internal metaenv table to be at the top of the stack on entry
 */
b8 Metaenvironment::processSections()
{
	using enum Section::Kind;

	TRACE("Metaenvironment::processSections()\n");
	SCOPED_INDENT;

	for (auto& s : section_list)
	{
		INFO("Section: ", s.kind, "\n");
	}

	LuaState& lua = lpp->lua;

	const s32 luabottom = lua.gettop();
	defer { lua.settop(luabottom); };

	// S1: metaenv
	
	const s32 metaenv_idx = lua.gettop();

	// Retrieve the expansion type from the lpp table

	lua.require("lpp"_str);
	const s32 lpp_idx = lua.gettop();

	lua.pushstring("MacroExpansion"_str);
	
	// S1: metaenv
	// S2: lpp
	// S3: "MacroExpansion"

	lua.gettable(lpp_idx);
	const s32 MacroExpansion_idx = lua.gettop();

	// S1: metaenv
	// S2: lpp
	// S3: MacroExpansion type

	// Get its type check function

	lua.pushstring("isTypeOf"_str);
	lua.gettable(MacroExpansion_idx);
	const s32 MacroExpansion_isTypeOf_idx = lua.gettop();

	// S1: metaenv
	// S2: lpp
	// S3: MacroExpansion type
	// S4: MacroExpansion.isTypeOf()
	
	// Get the environment's macro table

	lua.pushstring("macro_table"_str);
	lua.gettable(metaenv_idx);
	const s32 macro_table_idx = lua.gettop();

	// S1: metaenv
	// S2: lpp
	// S3: MacroExpansion type
	// S4: MacroExpansion.isTypeOf()
	// NOTE -- 2,3, and 4 are popped by defers...

	u64 section_idx = 0;
	for (current_section = section_list.head; 
		 current_section; 
		 current_section = current_section->next, section_idx += 1)
	{
		Section* s = current_section->data;
		expansions.pushTail({s->start_offset, output->cache.len});
		switch (s->kind)
		{
		case Macro:
			{
				lua.pushinteger(s->macro_idx);
				lua.gettable(macro_table_idx);
				const s32 macro_idx = lua.gettop();
				
				// S...
				// S5: macro function

				if (!lua.pcall(0, 1))
					return false;

				const s32 macro_result_idx = lua.gettop();

				// S...
				// S5: macro result

				if (!lua.isnil())
				{
					lua.pushvalue(MacroExpansion_isTypeOf_idx);
					lua.pushvalue(macro_result_idx);
					if (!lua.pcall(1, 1))
						return false;

					// S...
					// S5: macro result
					// S6: isTypeOf result

					if (!lua.toboolean())
					{
						lua.pop();
						
						// S...
						// S5: macro result -- a string, hopefully

						// If the macro returns something, we need to make sure its a string
						// and expand it into the buffer if so.
						if (!lua.isstring())
							return errorAt(s->start_offset, 
								"macro invocation returned a non-string value!");
					}
					else
					{
						lua.pop();
						lua.pushinteger(output->cache.len);

						// S...
						// S5: macro result -- a MacroExpansion
						// S6: current output length

						// Call the macro expansion so it records how each part
						// is expanded into the buffer
						if (!lua.pcall(1, 1))
							return false;
					}
					
					const s32 expanded_macro_idx = lua.gettop();

					// S...
					// S5: expanded macro -- string

					output->writeCache(lua.tostring(expanded_macro_idx));
				}

				lua.pop();

				// S...
			}
			break;

		case Document:
			output->writeCache(s->mem.asStr());
			break;
		}
	}

	// S same as before loop

	// cache the line offsets 
	output->cacheLineOffsets();

	for (auto& expansion : expansions)
	{
		printExpansion(input, output, expansion);
	}

	return true;
}

template<typename... T>
b8 Metaenvironment::errorAt(s32 offset, T... args)
{
	input->cacheLineOffsets();
	Source::Loc loc = input->getLoc(offset);
	ERROR(input->name, ":", loc.line, ":", loc.column, ": ", args..., "\n");
	return false;
}

extern "C"
{

/* ------------------------------------------------------------------------------------------------ metaenvironmentAddMacroSection
 */
LPP_LUAJIT_FFI_FUNC
void metaenvironmentAddMacroSection(MetaprogramContext* ctx, str indent, u64 start, u64 macro_idx)
{
	ctx->metaenv->addMacroSection(start, indent, macro_idx);
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentAddDocumentSection
 */
LPP_LUAJIT_FFI_FUNC
void metaenvironmentAddDocumentSection(MetaprogramContext* ctx, u64 start, str raw)
{
	ctx->metaenv->addDocumentSection(start, raw);
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentNewCursorAfterSection
 */
LPP_LUAJIT_FFI_FUNC
Cursor* metaenvironmentNewCursorAfterSection(MetaprogramContext* ctx)
{
	Metaenvironment* me = ctx->metaenv;

	if (!me->current_section->next)
		return nullptr;

	Cursor* cursor = me->cursors.add();
	cursor->creator = me->current_section->next;
	cursor->section = cursor->creator;
	cursor->range = cursor->section->data->mem.asStr();
	cursor->current_codepoint = utf8::decodeCharacter(cursor->range.bytes, cursor->range.len);
	return cursor;
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentDeleteCursor
 */
LPP_LUAJIT_FFI_FUNC
void metaenvironmentDeleteCursor(MetaprogramContext* ctx, Cursor* cursor)
{
	ctx->metaenv->cursors.remove(cursor);
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentGetMacroIndent
 */
LPP_LUAJIT_FFI_FUNC
str metaenvironmentGetMacroIndent(MetaprogramContext* ctx)
{
	Metaenvironment* me = ctx->metaenv;
	assert(sectionIsMacro(me->current_section));
	return me->current_section->data->macro_indent;
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentGetOutputSoFar
 */
LPP_LUAJIT_FFI_FUNC
str metaenvironmentGetOutputSoFar(MetaprogramContext* ctx)
{
	Metaenvironment* me = ctx->metaenv;
	return me->output->cache.asStr();
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentTrackExpansion
 */
LPP_LUAJIT_FFI_FUNC
void metaenvironmentTrackExpansion(MetaprogramContext* ctx, u64 from, u64 to)
{
	ctx->metaenv->expansions.pushTail({from, to});
}

/* ------------------------------------------------------------------------------------------------ cursorNextChar
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

	cursor->current_codepoint = utf8::decodeCharacter(cursor->range.bytes, cursor->range.len);
	return true;
}

/* ------------------------------------------------------------------------------------------------ cursorCurrentCodepoint
 */
LPP_LUAJIT_FFI_FUNC
u32  cursorCurrentCodepoint(Cursor* cursor)
{
	return cursor->current_codepoint.codepoint;
}

/* ------------------------------------------------------------------------------------------------ cursorInsertString
 */
LPP_LUAJIT_FFI_FUNC
b8 cursorInsertString(Cursor* cursor, str text)
{
	Section* s = cursor->section->data;
	if (s->kind != Section::Kind::Document)
		return false;
	
	assert(cursor->range.bytes >= s->mem.buffer);

	u64 cursor_offset = cursor->range.bytes - s->mem.buffer;

	if (!s->insertString(cursor_offset, text))
		return false;

	u8* new_pos = s->mem.buffer + cursor_offset + text.len;
	u64 new_len = (s->mem.buffer + s->mem.len) - new_pos;
	cursor->range = {new_pos, new_len};

	return true;
}

/* ------------------------------------------------------------------------------------------------ cursorGetRestOfSection
 */
LPP_LUAJIT_FFI_FUNC
str cursorGetRestOfSection(Cursor* cursor)
{
	assert(sectionIsDocument(cursorGetSection(cursor)));
	return cursor->range;
}

/* ------------------------------------------------------------------------------------------------ cursorGetSection
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* cursorGetSection(Cursor* cursor)
{
	return cursor->section;
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentGetNextSection
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* metaenvironmentGetNextSection(MetaprogramContext* ctx)
{
	return ctx->metaenv->current_section->next;
}

/* ------------------------------------------------------------------------------------------------ sectionGetString
 */
LPP_LUAJIT_FFI_FUNC
str sectionGetString(SectionNode* section)
{
	return section->data->mem.asStr();
}

/* ------------------------------------------------------------------------------------------------ sectionNext
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* sectionNext(SectionNode* section)
{
	return section->next;
}

/* ------------------------------------------------------------------------------------------------ sectionPrev
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* sectionPrev(SectionNode* section)
{
	return section->prev;
}

/* ------------------------------------------------------------------------------------------------ sectionIsMacro
 */
LPP_LUAJIT_FFI_FUNC
b8 sectionIsMacro(SectionNode* section)
{
	return section->data->kind == Section::Kind::Macro;
}

/* ------------------------------------------------------------------------------------------------ sectionIsDocument
 */
LPP_LUAJIT_FFI_FUNC
b8 sectionIsDocument(SectionNode* section)
{
	return section->data->kind == Section::Kind::Document;
}

/* ------------------------------------------------------------------------------------------------ sectionInsertString
 */
LPP_LUAJIT_FFI_FUNC
b8 sectionInsertString(SectionNode* section, u64 offset, str s)
{
	assert(sectionIsDocument(section));
	return section->data->insertString(offset, s);
}

/* ------------------------------------------------------------------------------------------------ sectionConsumeFromBeginning
 */
LPP_LUAJIT_FFI_FUNC
b8 sectionConsumeFromBeginning(SectionNode* section, u64 len)
{
	assert(sectionIsDocument(section));
	return section->data->consumeFromBeginning(len);
}

}

