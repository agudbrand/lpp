#include "metaenvironment.h"
#include "lpp.h"

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

/* ------------------------------------------------------------------------------------------------ Section::initDocument
 */
b8 Section::initMacro(u64 start_offset_, u64 macro_idx_, SectionNode* node_)
{
	start_offset = start_offset_;
	node = node_;
	macro_idx = macro_idx_;
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

	mem.reserve(s.len);
	mem.commit(s.len);

	mem::move(mem.buffer + offset + s.len, mem.buffer + offset, s.len);
	mem::copy(mem.buffer + offset, s.bytes, s.len);

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
	return true;
}

/* ------------------------------------------------------------------------------------------------ Metaenvironment::deinit
 */
void Metaenvironment::deinit()
{
	sections.destroy();
	cursors.destroy();
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
void Metaenvironment::addMacroSection(s64 start, u64 macro_idx)
{
	Section* s = sections.add();
	s->initMacro(start, macro_idx, section_list.pushTail(s));
}

/* ------------------------------------------------------------------------------------------------ Metaenvironment::processSections
 *  This expects the internal metaenv table to be at the top of the stack on entry
 */
b8 Metaenvironment::processSections()
{
	using enum Section::Kind;

	TRACE("Metaenvironment::processSections()\n");
	SCOPED_INDENT;

	LuaState* lua = &lpp->lua;
	
	lua->pushString("macro_table"_str);
	lua->getTable(lua->getTop()-1);
	defer { lua->pop(); };

	u64 section_idx = 0;
	for (current_section = section_list.head; 
		 current_section; 
		 current_section = current_section->next, section_idx += 1)
	{
		Section* s = current_section->data;
		switch (s->kind)
		{
		case Macro:
			lua->pushInteger(s->macro_idx);
			lua->getTable(-2);
			if (lua->pcall(0, 1))
				output->writeCache(lua->toString(-1));
			else 
				return false;
			lua->pop();
			break;

		case Document:
			output->writeCache(s->mem.asStr());
			break;
		}

	}

	output->cacheLineOffsets();

#if 0
	for (ExpansionMap& m : expansions)
	{
		Source::Loc old = input->getLoc(m.old_offset);
		Source::Loc nu  = output->getLoc(m.new_offset);
		INFO(old.line, ":", old.column, "(", m.old_offset, ") -> ", nu.line, ":", nu.column, "(", m.new_offset, ")\n");
	}
#endif

	return true;
}

extern "C"
{

/* ------------------------------------------------------------------------------------------------ metaenvironmentAddMacroSection
 */
void metaenvironmentAddMacroSection(MetaprogramContext* ctx, u64 start, u64 macro_idx)
{
	ctx->metaenv->addMacroSection(start, macro_idx);
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentAddDocumentSection
 */
void metaenvironmentAddDocumentSection(MetaprogramContext* ctx, u64 start, str raw)
{
	ctx->metaenv->addDocumentSection(start, raw);
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentNewCursorAfterSection
 */
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
void metaenvironmentDeleteCursor(MetaprogramContext* ctx, Cursor* cursor)
{
	ctx->metaenv->cursors.remove(cursor);
}

/* ------------------------------------------------------------------------------------------------ cursorNextChar
 */
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
u32  cursorCurrentCodepoint(Cursor* cursor)
{
	return cursor->current_codepoint.codepoint;
}

/* ------------------------------------------------------------------------------------------------ cursorInsertString
 */
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
str cursorGetRestOfSection(Cursor* cursor)
{
	assert(sectionIsDocument(cursorGetSection(cursor)));
	return cursor->range;
}

/* ------------------------------------------------------------------------------------------------ cursorGetSection
 */
SectionNode* cursorGetSection(Cursor* cursor)
{
	return cursor->section;
}

/* ------------------------------------------------------------------------------------------------ metaenvironmentGetNextSection
 */
SectionNode* metaenvironmentGetNextSection(MetaprogramContext* ctx)
{
	return ctx->metaenv->current_section->next;
}

/* ------------------------------------------------------------------------------------------------ sectionNext
 */
SectionNode* sectionNext(SectionNode* section)
{
	return section->next;
}

/* ------------------------------------------------------------------------------------------------ sectionPrev
 */
SectionNode* sectionPrev(SectionNode* section)
{
	return section->prev;
}

/* ------------------------------------------------------------------------------------------------ sectionIsMacro
 */
b8 sectionIsMacro(SectionNode* section)
{
	return section->data->kind == Section::Kind::Macro;
}

/* ------------------------------------------------------------------------------------------------ sectionIsDocument
 */
b8 sectionIsDocument(SectionNode* section)
{
	return section->data->kind == Section::Kind::Document;
}

/* ------------------------------------------------------------------------------------------------ sectionInsertString
 */
b8 sectionInsertString(SectionNode* section, u64 offset, str s)
{
	assert(sectionIsDocument(section));
	return section->data->insertString(offset, s);
}

}

