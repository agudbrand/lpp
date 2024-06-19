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

/* ================================================================================================ Section
 */
typedef DLinkedPool<Section> SectionPool;
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
 */
struct Metaprogram
{
	Lpp* lpp;

	SectionPool   sections;
	CursorPool    cursors;
	ExpansionList expansions;

	io::IO* instream;
	Source* input;
	Source* output;

	SectionNode* current_section;


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */


	b8   init(Lpp* lpp, io::IO* instream, Source* input, Source* output);
	void deinit();

	b8 run();

	b8 phase1();
	b8 phase2();
	b8 phase3();

	Section* insertSection(u64 idx);

	void addDocumentSection(u64 start, str s);
	void addMacroSection(s64 start, str indent, u64 macro_idx);

	b8 processSections();

	template<typename... T>
	b8 errorAt(s32 loc, T... args);
};

extern "C"
{

	// TODO(sushi) maybe someday put the lua api here.
	//             Its really not necessary and is just another place I have 
	//             to update declarations at.

}

#endif // _lpp_metaprogram_h
