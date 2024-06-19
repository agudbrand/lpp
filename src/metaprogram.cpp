#include "metaprogram.h"
#include "lpp.h"

#include "iro/fs/file.h"

static Logger logger = Logger::create("lpp.metaprog"_str, Logger::Verbosity::Trace);

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

/* ------------------------------------------------------------------------------------------------ Metaprogram::init
 */
b8 Metaprogram::init(Lpp* lpp, io::IO* instream, Source* input, Source* output)
{
	this->lpp = lpp;
	this->instream = instream;
	this->input = input;
	this->output = output;
	sections.init();
	cursors = CursorPool::create();
	expansions.init();
	return true;
}

/* ------------------------------------------------------------------------------------------------ Metaprogram::deinit
 */
void Metaprogram::deinit()
{
	sections.deinit();
	cursors.destroy();
	expansions.deinit();
	*this = {};
}

/* ------------------------------------------------------------------------------------------------ Metaprogram::addDocumentSection
 */
void Metaprogram::addDocumentSection(u64 start, str raw)
{
	auto node = sections.pushTail();
	node->data->initDocument(start, raw, node);
}

/* ------------------------------------------------------------------------------------------------ Metaprogram::addMacroSection
 */
void Metaprogram::addMacroSection(s64 start, str indent, u64 macro_idx)
{
	auto node = sections.pushTail();
	node->data->initMacro(start, indent, macro_idx, node);
}

static void printExpansion(Source* input, Source* output, Expansion& expansion)
{
	input->cacheLineOffsets();
	output->cacheLineOffsets();

	Source::Loc old = input->getLoc(expansion.from);
	Source::Loc nu  = output->getLoc(expansion.to);
	INFO(old.line, ":", old.column, "(", expansion.from, ") -> ", nu.line, ":", nu.column, "(", expansion.to, ")\n");
}

/* ------------------------------------------------------------------------------------------------
 */
b8 Metaprogram::run()
{
	using enum Section::Kind;

	LuaState& lua = lpp->lua;

	const s32 bottom = lua.gettop();
	defer { lua.settop(bottom); };

	// The parsed program, to be executed in phase 2.
	// TODO(sushi) get this to incrementally stream from the parser 
	//             directly to lua.load(). My whole IO setup is too dumb 
	//             to do that atm I think.
	io::Memory parsed_program;
	if (!parsed_program.open())
		return false;
	defer { parsed_program.close(); };

	// ~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~= Phase 1
	
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

	DEBUG(parsed_program.asStr(), "\n");

	// ~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~= Phase 2
	
	DEBUG("phase 2\n");

	// Execute the lua metacode to form the sections we process in the following phase.

	TRACE("loading parsed program\n");
	if (!lua.loadbuffer(parsed_program.asStr(), (char*)input->name.bytes))
	{
		ERROR("failed to load metaprogram into lua\n");
		return false;
	}
	const s32 I_metaprogram = lua.gettop();

	// Get the metaenvironment construction function and call it to 
	// make a new env.
	TRACE("constructing metaenvironment\n");
	if (!lua.require("metaenv"_str))
	{
		ERROR("failed to load metaenvironment module\n");
		return false;
	}

	lua.pushlightuserdata(this);
	if (!lua.pcall(1, 1))
	{
		ERROR("metaenvironment construction function failed\n");
		return false;
	}
	const s32 I_metaenv = lua.gettop();

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

	// Get the lpp module and set the proper metaprogram context. 
	TRACE("setting lpp module context\n");
	if (!lua.require("lpp"_str))
	{
		ERROR("failed to get lpp module for setting metaprogram context\n");
		return false;
	}
	const s32 I_lpp = lua.gettop();

	// Save the old context.
	lua.pushstring("context"_str);
	lua.gettable(I_lpp);
	const s32 I_prev_context = lua.gettop();

	// Ensure the context is restored.
	defer 
	{
		lua.pushstring("context"_str);
		lua.pushvalue(I_prev_context);
		lua.settable(I_lpp);
	};

	// Set the new context.
	lua.pushstring("context"_str);
	lua.pushlightuserdata(this);
	lua.settable(I_lpp);

	// Finally set the metaenvironment of the metacode and call it.
	TRACE("setting environment of metaprogram\n");
	lua.pushvalue(I_metaenv);
	if (!lua.setfenv(I_metaprogram))
	{
		ERROR("failed to set environment of metaprogram\n");
		return false;
	}

	TRACE("executing metaprogram\n");
	lua.pushvalue(I_metaprogram);
	if (!lua.pcall())
	{
		ERROR("failed to execute generated metaprogram\n");
		return false;
	}

	// ~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~= Phase 3
	
	DEBUG("phase 3\n");

	// Process each section generated by Phase 2, joining each Document section into a 
	// single buffer and performing macro expansions.
	
	// Retrieve MacroExpansion.isTypeOf
	TRACE("getting MacroExpansion.isTypeOf\n");
	lua.pushstring("MacroExpansion"_str);
	lua.gettable(I_lpp);
	const s32 I_MacroExpansion = lua.gettop();

	lua.pushstring("isTypeOf"_str);
	lua.gettable(I_MacroExpansion);
	const s32 I_MacroExpansion_isTypeOf = lua.gettop();

	// Retrieve the __metaenv table.
	TRACE("getting '__metaenv'\n");
	lua.pushstring("__metaenv"_str);
	lua.gettable(I_metaenv);
	const s32 I_metaenv_table = lua.gettop();

	// Get the metaenv's macro table.
	TRACE("getting metaenvironment's macro table\n");
	lua.pushstring("macro_table"_str);
	lua.gettable(I_metaenv_table);
	const s32 I_macro_table = lua.gettop();

	// Loop over each section, joining document sections and expanded macros.
	TRACE("beginning section loop\n");
	u64 section_idx = 0;
	for (current_section = sections.headNode();
		 current_section;
		 current_section = current_section->next)
	{
		Section* section = current_section->data;
		
		// Any section is always an expansion into the resulting buffer.
		expansions.pushTail({section->start_offset, output->cache.len});

		switch (section->kind)
		{
		case Document:
			TRACE("found Document section\n");
			// Simple write to the output cache.
			output->writeCache(section->mem.asStr());
			break;

		case Macro:
			{
				TRACE("found Macro section\n");
				// Get the macro and execute it.
				lua.pushinteger(section->macro_idx);
				lua.gettable(I_macro_table);
				const s32 I_macro = lua.gettop();

				TRACE("invoking macro\n");
				if (!lua.pcall(0, 1))
					return false;
				const s32 I_macro_result = lua.gettop();

				// Figure out what, if anything, the macro returned.
				if (!lua.isnil())
				{
					TRACE("macro returned a value\n");
					lua.pushvalue(I_MacroExpansion_isTypeOf);
					lua.pushvalue(I_macro_result);
					if (!lua.pcall(1, 1))
						return false;

					b8 is_expansion = lua.toboolean();
					lua.pop();

					if (is_expansion)
					{
						// Not a macro expansion, need to make sure its a string instead.
						lua.pop();

						if (!lua.isstring())
							return errorAt(section->start_offset,
								"macro invocation returned a non-string value!");
						TRACE("macro returned a string\n");
					}
					else
					{
						lua.pop();
						TRACE("macro returned a MacroExpansion\n");

						// Push where we currently are in the output and then call
						// the MacroExpansion so that it may report how each part of itself
						// is expanded into the buffer.
						lua.pushinteger(output->cache.len);
						if (!lua.pcall(1, 1))
							return false;
					}
					output->writeCache(lua.tostring());
				}

				lua.pop();
			}
			break;
		}
	}

	output->cacheLineOffsets();

	for (auto& expansion : expansions)
	{
		printExpansion(input, output, expansion);
	}

	return true;
}

template<typename... T>
b8 Metaprogram::errorAt(s32 offset, T... args)
{
	input->cacheLineOffsets();
	Source::Loc loc = input->getLoc(offset);
	ERROR(input->name, ":", loc.line, ":", loc.column, ": ", args..., "\n");
	return false;
}

extern "C"
{

// Forward decls, as needed.
b8 sectionIsDocument(SectionNode* section);
b8 sectionIsMacro(SectionNode* section);

/* ------------------------------------------------------------------------------------------------ 
 */
LPP_LUAJIT_FFI_FUNC
void metaprogramAddMacroSection(Metaprogram* mp, str indent, u64 start, u64 macro_idx)
{
	mp->addMacroSection(start, indent, macro_idx);
}

/* ------------------------------------------------------------------------------------------------ 
 */
LPP_LUAJIT_FFI_FUNC
void metaprogramAddDocumentSection(Metaprogram* mp, u64 start, str raw)
{
	mp->addDocumentSection(start, raw);
}

/* ------------------------------------------------------------------------------------------------ 
 */
LPP_LUAJIT_FFI_FUNC
Cursor* metaprogramNewCursorAfterSection(Metaprogram* mp)
{
	if (!mp->current_section->next)
		return nullptr;

	Cursor* cursor = mp->cursors.add();
	cursor->creator = mp->current_section->next;
	cursor->section = cursor->creator;
	cursor->range = cursor->section->data->mem.asStr();
	cursor->current_codepoint = utf8::decodeCharacter(cursor->range.bytes, cursor->range.len);
	return cursor;
}

/* ------------------------------------------------------------------------------------------------ 
 */
LPP_LUAJIT_FFI_FUNC
void metaprogramDeleteCursor(Metaprogram* mp, Cursor* cursor)
{
	mp->cursors.remove(cursor);
}

/* ------------------------------------------------------------------------------------------------ 
 */
LPP_LUAJIT_FFI_FUNC
str metaprogramGetMacroIndent(Metaprogram* mp)
{
	assert(sectionIsMacro(mp->current_section));
	return mp->current_section->data->macro_indent;
}

/* ------------------------------------------------------------------------------------------------ 
 */
LPP_LUAJIT_FFI_FUNC
str metaprogramGetOutputSoFar(Metaprogram* mp)
{
	return mp->output->cache.asStr();
}

/* ------------------------------------------------------------------------------------------------ 
 */
LPP_LUAJIT_FFI_FUNC
void metaprogramTrackExpansion(Metaprogram* mp, u64 from, u64 to)
{
	mp->expansions.pushTail({from, to});
}

/* ------------------------------------------------------------------------------------------------ 
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

/* ------------------------------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
u32  cursorCurrentCodepoint(Cursor* cursor)
{
	return cursor->current_codepoint.codepoint;
}

/* ------------------------------------------------------------------------------------------------ 
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

/* ------------------------------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* cursorGetSection(Cursor* cursor)
{
	return cursor->section;
}

/* ------------------------------------------------------------------------------------------------ 
 */
LPP_LUAJIT_FFI_FUNC
str cursorGetRestOfSection(Cursor* cursor)
{
	assert(sectionIsDocument(cursorGetSection(cursor)));
	return cursor->range;
}

/* ------------------------------------------------------------------------------------------------ 
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* metaprogramGetNextSection(Metaprogram* mp)
{
	return mp->current_section->next;
}

/* ------------------------------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
str sectionGetString(SectionNode* section)
{
	return section->data->mem.asStr();
}

/* ------------------------------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* sectionNext(SectionNode* section)
{
	return section->next;
}

/* ------------------------------------------------------------------------------------------------
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

