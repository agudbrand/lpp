/*
 *  The environment in which a metaprogram is processed into 
 *  its final output.
 */

#ifndef _lpp_metaenvironment_h
#define _lpp_metaenvironment_h

#include "common.h"
#include "containers/array.h"
#include "containers/pool.h"

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
		
		u64 start_offset;
		Kind kind;

		io::Memory mem;
		str range;
	};

	Array<Section> sections;
	
    /* ============================================================================================ Metaenvironment::ExpansionMap
     */
	struct ExpansionMap
	{
		u64 old_range[2];
		u64 new_range[2];
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

	void add_document_section(u64 start, str s);
	void add_macro_section(s64 start);

	b8 process_sections();
};

extern "C"
{

void metaenvironment_add_macro_section(MetaprogramContext* ctx, u64 start);
void metaenvironment_add_document_section(MetaprogramContext* ctx, u64 start, str s);

Metaenvironment::Cursor* metaenvironment_new_cursor_after_section(MetaprogramContext* ctx);
void metaenvironment_delete_cursor(MetaprogramContext* ctx, Metaenvironment::Cursor* cursor);

b8  metaenvironment_cursor_next_char(MetaprogramContext* ctx, Metaenvironment::Cursor* cursor);
u32 metaenvironment_cursor_current_codepoint(MetaprogramContext* ctx, Metaenvironment::Cursor* cursor);

struct SourceLoc
{
	u64 line;
	u64 column;
};

// TODO(sushi) this might not make sense?
SourceLoc metaenvironment_cursor_source_loc(MetaprogramContext* ctx, Metaenvironment::Cursor* cursor);

}

#endif // _lpp_metaenvironment_h
