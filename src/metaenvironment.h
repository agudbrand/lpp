/*
 *  The environment in which a metaprogram is processed into 
 *  its final output.
 */

#ifndef _lpp_metaenvironment_h
#define _lpp_metaenvironment_h

#include "iro/common.h"
#include "iro/containers/array.h"
#include "iro/containers/pool.h"

#include "source.h"

struct MetaprogramContext;
struct Lpp;

using namespace iro;

/* ================================================================================================ Metaenvironment
 */
struct Metaenvironment
{
    /* ============================================================================================ Metaenvironment::Section
     */
	struct Section
	{
		enum class Kind : u32
		{
			Document,
			Macro,
		};
		
		u64 start_offset = -1;
		Kind kind = Kind::Document;

		io::Memory mem = {};
		str range = {};
	};

	Array<Section> sections;
	
    /* ============================================================================================ Metaenvironment::ExpansionMap
     */
	struct ExpansionMap
	{
		u64 old_offset;
		u64 new_offset;
	};

    /* ============================================================================================ Metaenvironment::Cursor
     */
	struct Cursor
	{
		u32 section_idx;
		str range;
		utf8::Codepoint current_codepoint;
	};

	Pool<Cursor> cursors;

	Array<ExpansionMap> expansions;

	Source* input;
	Source* output;

	Lpp* lpp;

	u64 section_idx;


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */


	b8   init(Lpp* lpp, Source* input, Source* output);
	void deinit();

	Section* insertSection(u64 idx);

	void addDocumentSection(u64 start, str s);
	void addMacroSection(s64 start);

	b8 processSections();
};

extern "C"
{

void metaenvironmentAddMacroSection(MetaprogramContext* ctx, u64 start);
void metaenvironmentAddDocumentSection(MetaprogramContext* ctx, u64 start, str s);

Metaenvironment::Cursor* metaenvironmentNewCursorAfterSection(MetaprogramContext* ctx);
void metaenvironmentDeleteCursor(MetaprogramContext* ctx, Metaenvironment::Cursor* cursor);

b8  metaenvironmentCursorNextChar(MetaprogramContext* ctx, Metaenvironment::Cursor* cursor);
u32 metaenvironmentCursorCurrentCodepoint(MetaprogramContext* ctx, Metaenvironment::Cursor* cursor);

struct SourceLoc
{
	u64 line;
	u64 column;
};

// TODO(sushi) this might not make sense?
SourceLoc metaenvironmentCursorSourceLoc(MetaprogramContext* ctx, Metaenvironment::Cursor* cursor);

b8 metaenvironmentCursorInsertString(MetaprogramContext* ctx, Metaenvironment::Cursor* cursor, str text);

}

#endif // _lpp_metaenvironment_h
