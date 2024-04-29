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

/* ------------------------------------------------------------------------------------------------ Metaenvironment::add_document_section
 */
void Metaenvironment::add_document_section(u64 start, str raw)
{
	Section* s = sections.push();
	s->kind = Section::Kind::Document;
	s->start_offset = start;
	s->mem.open(raw.len);
	s->mem.write(raw);
	s->range = s->mem.as_str();
}

/* ------------------------------------------------------------------------------------------------ Metaenvironment::add_macro_section
 */
void Metaenvironment::add_macro_section(s64 start)
{
	Section* s = sections.push();
	s->kind = Section::Kind::Macro;
	s->start_offset = start;
}

/* ------------------------------------------------------------------------------------------------ Metaenvironment::process_sections
 *  This expects the internal metaenv table to be at the top of the stack on entry
 */
b8 Metaenvironment::process_sections()
{
	using enum Section::Kind;

	SCOPED_INDENT;

	LuaState* lua = &lpp->lua;
	
	lua->pushstring("data"_str);
	lua->gettable(lua->gettop()-1);
	defer { lua->pop(); };

	s32 count = sections.len();
	for (s32 i = 0; i < count; i++)
	{
		section_idx = i;
		Section* current_section = &sections[i];

		lua->pushinteger(i+1);
		lua->gettable(-2);

		// lua->stack_dump(1);

		ExpansionMap* map = expansions.push();
		map->old_range[0] = current_section->start_offset;
		map->new_range[0] = output->cache.len;

		switch (current_section->kind)
		{
		case Macro:
			if (lua->pcall(0, 1))
			{
				str res = lua->tostring(-1);
				output->write_cache(res);
			}
			else
				return false;
			break;

		case Document:
			output->write_cache(current_section->range);
			break;
		}
		
		lua->pop();
	}

	output->cache_line_offsets();

	for (ExpansionMap& m : expansions)
	{
		Source::Loc old = input->get_loc(m.old_range[0]);
		Source::Loc nu  = output->get_loc(m.new_range[0]);
		INFO(old.line, ":", old.column, "(", m.old_range[0], ") -> ", nu.line, ":", nu.column, "(", m.new_range[0], ")\n");
	}


	return true;
}

extern "C"
{

/* ------------------------------------------------------------------------------------------------ metaenvironment_add_macro_section
 */
void metaenvironment_add_macro_section(MetaprogramContext* ctx, u64 start)
{
	ctx->metaenv->add_macro_section(start);
}

/* ------------------------------------------------------------------------------------------------ metaenvironment_add_document_section
 */
void metaenvironment_add_document_section(MetaprogramContext* ctx, u64 start, str raw)
{
	ctx->metaenv->add_document_section(start, raw);
}

static Metaenvironment::Section* find_next_document_section(Metaenvironment* me, u64* idx)
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

/* ------------------------------------------------------------------------------------------------ metaenvironment_new_cursor_after_section
 */
Metaenvironment::Cursor* metaenvironment_new_cursor_after_section(MetaprogramContext* ctx)
{
	Metaenvironment* me = ctx->metaenv;

	if (me->section_idx == me->sections.len() - 1)
	{
		return nullptr;
	}

	u64 place_idx = me->section_idx + 1;
	Metaenvironment::Section* s = find_next_document_section(me, &place_idx);
	if (!s)
		return nullptr;

	Metaenvironment::Cursor* cursor = me->cursors.add();
	cursor->section_idx = place_idx;
	cursor->range = s->range;
	cursor->current_codepoint = cursor->range.advance();
	return cursor;
}

/* ------------------------------------------------------------------------------------------------ metaenvironment_delete_cursor
 */
void metaenvironment_delete_cursor(MetaprogramContext* ctx, Metaenvironment::Cursor* cursor)
{
	ctx->metaenv->cursors.remove(cursor);
}

/* ------------------------------------------------------------------------------------------------ metaenvironment_cursor_next_char
 */
b8 metaenvironment_cursor_next_char(MetaprogramContext* ctx, Metaenvironment::Cursor* cursor)
{
	Metaenvironment* me = ctx->metaenv;
	if (cursor->range.len == 0)
	{
		u64 new_idx = cursor->section_idx + 1;
		Metaenvironment::Section* s = find_next_document_section(me, &new_idx);
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

/* ------------------------------------------------------------------------------------------------ metaenvironment_cursor_current_codepoint
 */
u32  metaenvironment_cursor_current_codepoint(MetaprogramContext* ctx, Metaenvironment::Cursor* cursor)
{
	return cursor->current_codepoint.codepoint;
}

}

