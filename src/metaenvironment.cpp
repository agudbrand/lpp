#include "metaenvironment.h"
#include "lpp.h"

static Logger logger = Logger::create("metaenv"_str, Logger::Verbosity::Trace);

/* ------------------------------------------------------------------------------------------------ Metaenvironment::init
 */
b8 Metaenvironment::init(Lpp* lpp, Source* input, Source* output)
{
	this->lpp = lpp;
	this->input = input;
	this->output = output;
	sections = Array<Section>::create();
	expansions = Array<ExpansionMap>::create();
	cursors = Pool<Cursor>::create();
	return true;
}

/* ------------------------------------------------------------------------------------------------ Metaenvironment::deinit
 */
void Metaenvironment::deinit()
{
	sections.destroy();
	expansions.destroy();
	cursors.destroy();
	*this = {};
}

/* ------------------------------------------------------------------------------------------------ Metaenvironment::addDocumentSection
 */
void Metaenvironment::addDocumentSection(u64 start, str raw)
{
	Section* s = sections.push();
	s->kind = Section::Kind::Document;
	s->start_offset = start;
	s->mem.open(raw.len);
	s->mem.write(raw);
	s->range = s->mem.asStr();
}

/* ------------------------------------------------------------------------------------------------ Metaenvironment::addMacroSection
 */
void Metaenvironment::addMacroSection(s64 start)
{
	Section* s = sections.push();
	s->kind = Section::Kind::Macro;
	s->start_offset = start;
}

/* ------------------------------------------------------------------------------------------------ Metaenvironment::processSections
 *  This expects the internal metaenv table to be at the top of the stack on entry
 */
b8 Metaenvironment::processSections()
{
	using enum Section::Kind;

	SCOPED_INDENT;

	LuaState* lua = &lpp->lua;
	
	lua->pushString("data"_str);
	lua->getTable(lua->getTop()-1);
	defer { lua->pop(); };

	for (s32 i = 0; i < sections.len(); i++)
	{
		section_idx = i;
		Section* current_section = &sections[i];

		lua->pushInteger(i+1);
		lua->getTable(-2);

		// lua->stack_dump(1);

		ExpansionMap* map = expansions.push();
		map->old_offset = current_section->start_offset;
		map->new_offset = output->cache.len;

		switch (current_section->kind)
		{
		case Macro:
			if (lua->pcall(0, 1))
			{
				str res = lua->toString(-1);
				output->writeCache(res);
			}
			else
				return false;
			break;

		case Document:
			output->writeCache(current_section->range);
			break;
		}
		
		lua->pop();
	}

	output->cacheLineOffsets();

	for (ExpansionMap& m : expansions)
	{
		Source::Loc old = input->getLoc(m.old_offset);
		Source::Loc nu  = output->getLoc(m.new_offset);
		INFO(old.line, ":", old.column, "(", m.old_offset, ") -> ", nu.line, ":", nu.column, "(", m.new_offset, ")\n");
	}


	return true;
}

extern "C"
{

/* ------------------------------------------------------------------------------------------------ metaenvironmentAddMacroSection
 */
void metaenvironmentAddMacroSection(MetaprogramContext* ctx, u64 start)
{
	ctx->metaenv->addMacroSection(start);
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentAddDocumentSection
 */
void metaenvironmentAddDocumentSection(MetaprogramContext* ctx, u64 start, str raw)
{
	ctx->metaenv->addDocumentSection(start, raw);
}

static Metaenvironment::Section* findNextDocumentSection(Metaenvironment* me, u64* idx)
{
	if (*idx >= me->sections.len())
		return nullptr;

	Metaenvironment::Section* s = &me->sections[*idx];
	for (;;)
	{
		if (s->kind == Metaenvironment::Section::Kind::Document)
			break;
		*idx += 1;
		if (*idx >= me->sections.len())
			return nullptr;
		s = &me->sections[*idx];
	}

	return s;
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentNewCursorAfterSection
 */
Metaenvironment::Cursor* metaenvironmentNewCursorAfterSection(MetaprogramContext* ctx)
{
	Metaenvironment* me = ctx->metaenv;

	if (me->section_idx == me->sections.len() - 1)
	{
		return nullptr;
	}

	u64 place_idx = me->section_idx + 1;
	Metaenvironment::Section* s = findNextDocumentSection(me, &place_idx);
	if (!s)
		return nullptr;

	Metaenvironment::Cursor* cursor = me->cursors.add();
	cursor->section_idx = place_idx;
	cursor->range = s->range;
	cursor->current_codepoint = cursor->range.advance();
	return cursor;
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentDeleteCursor
 */
void metaenvironmentDeleteCursor(MetaprogramContext* ctx, Metaenvironment::Cursor* cursor)
{
	ctx->metaenv->cursors.remove(cursor);
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentCursorNextChar
 */
b8 metaenvironmentCursorNextChar(MetaprogramContext* ctx, Metaenvironment::Cursor* cursor)
{
	Metaenvironment* me = ctx->metaenv;
	if (cursor->range.len == 0)
	{
		u64 new_idx = cursor->section_idx + 1;
		Metaenvironment::Section* s = findNextDocumentSection(me, &new_idx);
		if (!s)
			return false;
		cursor->section_idx = new_idx;
		cursor->range = s->range;
		cursor->current_codepoint = cursor->range.advance();
		return true;
	}

	cursor->current_codepoint = cursor->range.advance();
	return true;
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentCursorCurrentCodepoint
 */
u32  metaenvironmentCursorCurrentCodepoint(MetaprogramContext* ctx, Metaenvironment::Cursor* cursor)
{
	return cursor->current_codepoint.codepoint;
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentCursorCurrentCodepoint
 */
b8 metaenvironmentCursorInsertString(
		MetaprogramContext* ctx, 
		Metaenvironment::Cursor* cursor, 
		str text)
{
	// this is horrible and should definitely be cleaned up later 
	Metaenvironment* me = ctx->metaenv;
	me->sections.insert(cursor->section_idx+1);
	me->sections.insert(cursor->section_idx+1);
	Metaenvironment::Section* exp  = &me->sections[cursor->section_idx+1];
	Metaenvironment::Section* post = &me->sections[cursor->section_idx+2];
	Metaenvironment::Section* pre  = &me->sections[cursor->section_idx];
	post->mem.open(cursor->range.len);
	exp->mem.open(text.len);
	post->mem.write(cursor->range);
	pre->mem.len = cursor->range.bytes - pre->mem.buffer;
	exp->mem.write(text);
	exp->start_offset = pre->mem.len + pre->start_offset;
	exp->range = exp->mem.asStr();
	post->range = post->mem.asStr();
	post->start_offset = exp->mem.len + exp->start_offset;
	pre->range = pre->mem.asStr();
	cursor->range = exp->range;
	cursor->section_idx += 1;
	cursor->current_codepoint = cursor->range.advance();
	return true;
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentCursorGetRestOfSection
 */
str metaenvironmentCursorGetRestOfSection(
		MetaprogramContext* ctx, 
		Metaenvironment::Cursor* cursor)
{
	return cursor->range;
}

}

