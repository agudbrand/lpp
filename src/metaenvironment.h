/*
 *  The environment in which a metaprogram is processed into 
 *  its final output.
 */

#ifndef _lpp_metaenvironment_h
#define _lpp_metaenvironment_h

#include "iro/common.h"
#include "iro/containers/array.h"
#include "iro/containers/pool.h"
#include "iro/containers/list.h"

#include "source.h"

struct MetaprogramContext;
struct Lpp;

using namespace iro;

struct Section;
struct Cursor;

/* ================================================================================================ Section
 */
typedef Pool<Section> SectionPool;
typedef DList<Section> SectionList;
typedef SectionList::Node SectionNode;

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


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */


	b8 initDocument(u64 start_offset, str raw, SectionNode* node);
	b8 initMacro(u64 start_offset, u64 macro_idx, SectionNode* node);
	void deinit();

	b8 insertString(u64 start, str s);
};

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

/* ================================================================================================ Metaenvironment
 */
struct Metaenvironment
{
	SectionPool sections;
	SectionList section_list;

	CursorPool  cursors;

	Source* input;
	Source* output;

	Lpp* lpp;

	SectionNode* current_section;


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */


	b8   init(Lpp* lpp, Source* input, Source* output);
	void deinit();

	Section* insertSection(u64 idx);

	void addDocumentSection(u64 start, str s);
	void addMacroSection(s64 start, u64 macro_idx);

	b8 processSections();
};

extern "C"
{

void metaenvironmentAddMacroSection(MetaprogramContext* ctx, u64 start, u64 macro_idx);
void metaenvironmentAddDocumentSection(MetaprogramContext* ctx, u64 start, str s);

Cursor* metaenvironmentNewCursorAfterSection(MetaprogramContext* ctx);
void metaenvironmentDeleteCursor(MetaprogramContext* ctx, Cursor* cursor);

b8  cursorNextChar(Cursor* cursor);
b8  cursorNextSection(Cursor* cursor);
u32 cursorCurrentCodepoint(Cursor* cursor);
b8  cursorInsertString(Cursor* cursor, str text);
str cursorGetRestOfSection(Cursor* cursor);
SectionNode* cursorGetSection(Cursor* cursor);

SectionNode* metaenvironmentGetNextSection(MetaprogramContext* ctx);

SectionNode* sectionNext(SectionNode* section);
SectionNode* sectionPrev(SectionNode* section);

b8 sectionIsMacro(SectionNode* section);
b8 sectionIsDocument(SectionNode* section);

b8 sectionInsertString(SectionNode* section, u64 offset, str s);

}

#endif // _lpp_metaenvironment_h
