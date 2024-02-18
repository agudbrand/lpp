#include "lpp.h"
#include "common.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "ctype.h"

// pretty terminal colors
typedef enum 
{ 
	Color_Black = 30,
	Color_Red,
	Color_Green,
	Color_Yellow,
	Color_Blue,
	Color_Magenta,
	Color_Cyan,
	Color_White,

	Color_RESET = 0,
} Color;

void putcolor(lppContext* lpp, Color color)
{
	if (lpp->use_color)
		fprintf(stdout, "\e[%im", color);
}

void fatal_error(lppContext* lpp, s64 line, s64 column, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	putcolor(lpp, Color_Blue);
	fprintf(stdout, "%s", lpp->input_file_name);
	putcolor(lpp, Color_RESET);
	fprintf(stdout, ":%li:%li: ", line, column);
	putcolor(lpp, Color_Red);
	fprintf(stdout, "fatal_error");
	putcolor(lpp, Color_RESET);
	fprintf(stdout, ": ");
	vfprintf(stdout, fmt, args);
	exit(1);
}

void error(lppContext* lpp, s64 line, s64 column, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	putcolor(lpp, Color_Blue);
	fprintf(stdout, "%s", lpp->input_file_name);
	putcolor(lpp, Color_RESET);
	fprintf(stdout, ":%li:%li: ", line, column);
	putcolor(lpp, Color_Red);
	fprintf(stdout, "error");
	putcolor(lpp, Color_RESET);
	fprintf(stdout, ": ");
	vfprintf(stdout, fmt, args);
}

void warning(lppContext* lpp, s64 line, s64 column, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	putcolor(lpp, Color_Blue);
	fprintf(stdout, "%s", lpp->input_file_name);
	putcolor(lpp, Color_RESET);
	fprintf(stdout, ":%li:%li: ", line, column);
	putcolor(lpp, Color_Yellow);
	fprintf(stdout, "warning");
	putcolor(lpp, Color_RESET);
	fprintf(stdout, ": ");
	vfprintf(stdout, fmt, args);
}


static void initialize_lua(lppContext* lpp)
{
	lpp->L = lua_open();
	luaL_openlibs(lpp->L);
}

//b8 luacall(u32 n_returns)
//{
//	if (lua_pcall(lpp->L, 0, n_returns, 0))
//	{
//		error("%s\n", lua_tostring(lpp->L, -1));
//		lua_pop(lpp->L, 1);
//		return 0;
//	}
//
//	return 1;
//}

void
print_lua_value(lua_State* L, int idx) {
	int t = lua_type(L, idx);
	switch(t) {
		case LUA_TTABLE: {
			printf("lua_to_str8 does not handle tables, `print_table` should have been called for this element.");
		} break;
		case LUA_TNIL: printf("nil"); return;
		case LUA_TNUMBER: printf("%f", lua_tonumber(L, idx)); return;
		case LUA_TSTRING: printf("\"%s\"", lua_tostring(L, idx)); return;
		case LUA_TBOOLEAN: printf(lua_toboolean(L, idx) ? "true" : "false"); return;
		case LUA_TTHREAD: printf("<lua thread>"); return;
		case LUA_TFUNCTION: printf("<lua function>"); return;
		default: {
			printf("unhandled lua type in lua_to_str8: %s", lua_typename(L, t));
		} break;
	}
}

void
print_lua_table_interior(lua_State* L, int idx, u32 layers, u32 depth) {
#define do_indent \
	for(u32 i = 0; i < layers; i++) printf(" ");
	
	if (depth == layers)
	{
		do_indent;
		printf("...\n");
		return;
	}

	lua_pushvalue(L, idx);
	lua_pushnil(L);
	while(lua_next(L, -2)) {
		int kt = lua_type(L, -2);
		int vt = lua_type(L, -1);
		do_indent;
		print_lua_value(L, -2);
		printf(" = ");
		if(vt == LUA_TTABLE) {
			printf("{\n");
			print_lua_table_interior(L, -1, layers + 1, depth);
			do_indent;
			printf("}\n");
		} else {
			print_lua_value(L, -1);
			printf("\n");
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

#undef do_indent
}

void
print_lua_table(lua_State* L, int idx, u32 depth) {
	printf("{\n");
	print_lua_table_interior(L, idx, 1, depth);
	printf("}\n");
}

void
stack_dump(lua_State* L) {
	printf("-------------\n");
	int top = lua_gettop(L);
	for(int i = 1; i <= top; i++) {
		int t = lua_type(L, i);
		printf("i %i: ", i);
		if(t==LUA_TTABLE) {
			print_lua_table(L, i, 4);
		} else {
			print_lua_value(L, i);
		}
		printf("\n");
	}
}

#include "metaenv.carray"

//void load_metaprogram(lppContext* lpp)
//{
//	if (luaL_loadbuffer(lpp->L, (char*)lpp->metaprogram.s, lpp->metaprogram.count, lpp->input_file_name))
//	{
//		fprintf(stdout, "%s\n", lua_tostring(lpp->L, -1));
//		lua_pop(lpp->L, 1);
//		return;
//	}
//#define METAPROG_IDX 1
//
//	// TODO(sushi) replace with C array 
//	//             xxd -i src/metaenv.lua
//	if (luaL_loadfile(lpp->L, "src/metaenv.lua"))
//	{
//		fprintf(stdout, "%s\n", lua_tostring(lpp->L, -1));
//		lua_pop(lpp->L, 1);
//		return;
//	}
//
//	if (!luacall(1))
//		fatal_error("metaenv failed to run\n");
//
//	if (!lua_setfenv(lpp->L, METAPROG_IDX))
//		fatal_error("failed to set environment of metaprogram");
//}
//
//void run_metaprogram()
//{
//	luacall(0);	
//}

//static void build_metaprogram(lppContext* lpp)
//{
//	lpp->metaprogram = dstr_create(0);
//	dstr* mp = &lpp->metaprogram;
//
//	for (u32 i = 0; i < lpp->n_parts; i++)
//	{
//		Part* p = lpp->parts + i;
//		
//		switch (p->type)
//		{
//			case PartType_LuaLine:
//			case PartType_LuaBlock:
//				dstr_push_str(mp, p->s);
//				dstr_push_char(mp, '\n');
//			break;
//			case PartType_LuaValue:
//				dstr_push_cstr(mp, "__VAL(");
//				dstr_push_str(mp, p->s);
//				dstr_push_cstr(mp, ")\n");
//			break;
//			case PartType_LuaMacro:
//			{
//				dstr_push_cstr(mp, "__C(");
//
//				lpp->cursor = p->s.s;
//
//				str name;
//				name.s = lpp->cursor;
//
//				while (!at('(') && !isspace(current())) advance();
//
//				name.count = lpp->cursor - name.s;
//
//				dstr_push_str(mp, name);
//				dstr_push_char(mp, '(');
//
//				if (!at('(')) while (!at('(')) advance();
//				advance();
//
//				str arg;
//
//				u32 nesting = 1;
//				while (!eof())
//				{
//					skip_whitespace();
//					arg.s = lpp->cursor;
//					
//					while (!at(','))
//					{
//						if (at(')')) 
//						{
//							nesting -= 1;
//							if (!nesting) break;
//						}
//						else if (at('(')) nesting += 1;
//						advance();
//					}
//
//					arg.count = lpp->cursor - arg.s;
//					dstr_push_char(mp, '"');
//					dstr_push_str(mp, arg);
//					dstr_push_char(mp, '"');
//
//					if (at(')')) break;
//
//					dstr_push_cstr(mp, ",");
//					advance();
//				}
//
//				dstr_push_cstr(mp, "))\n");
//			}
//			break;
//			case PartType_LuaMacroStringed:
//			{
//				dstr_push_cstr(mp, "__C(");
//
//				lpp->cursor = p->s.s;
//
//				str name;
//				name.s = lpp->cursor;
//
//				while (!at('"') && !isspace(current())) advance();
//
//				name.count = lpp->cursor - name.s;
//
//				dstr_push_str(mp, name);
//				dstr_push_char(mp, '(');
//
//				str arg;
//
//				if (!at('"')) skip_whitespace();
//				arg.s = lpp->cursor;
//				advance();
//				while (!at('"')) advance();
//				advance();
//
//				arg.count = lpp->cursor - arg.s;
//				dstr_push_str(mp, arg);
//				dstr_push_cstr(mp, "))\n");
//			}
//			break;
//			case PartType_C:
//				dstr_push_cstr(mp, "__C(\"");
//				// need to sanitize
//				for (u32 i = 0; i < p->s.count; i++) 
//				{
//					u8 c = p->s.s[i];
//					if (iscntrl(c))
//					{
//						dstr_push_char(mp, '\\');
//						dstr_push_u8(mp, c);
//					}
//					else if (c == '"')
//					{
//						dstr_push_char(mp, '\\');
//						dstr_push_u8(mp, '"');
//					}
//					else if (c == '\\')
//					{
//						dstr_push_char(mp, '\\');
//						dstr_push_char(mp, '\\');
//					}
//					else 
//						dstr_push_char(mp, c);
//				}
//				dstr_push_cstr(mp, "\")\n");
//			break;
//		}
//	}
//
//	dstr_push_cstr(mp, "io.write(__FINAL())\n");
//
//	fprintf(stdout, "%.*s", mp->count, mp->s);
//}

void build_metprogram(lppContext* lpp)
{
	lpp->metaprogram = dstr_create(0);
	dstr* mp = &lpp->metaprogram;

	Lexer* l = &lpp->lexer;
	
	Token* tokens = l->tokens;
	u64* lua_tokens = l->lua_tokens;

	for (s32 i = 0; i < l->lua_token_count; i++)
	{
		u64 lua_token_idx = lua_tokens[i];
		Token* lua_token = tokens + lua_token_idx;

		if (lua_token_idx)
		{

		}
		


	}
}

void lpp_run(lppContext* lpp)
{

	FILE* input_file = fopen(lpp->input_file_name, "r");
	if (!input_file)
	{
		perror("fopen");
		fatal_error(lpp, -1, -1, "failed to open input file\n");
	}

	fseek(input_file, 0, SEEK_END);
	u64 file_size = ftell(input_file);
	fseek(input_file, 0, SEEK_SET);

	u8* input_buffer = malloc(file_size);
	u64 bytes_read = fread(input_buffer, file_size, 1, input_file);
	if (ferror(input_file))
	{
		perror("fread");
		fatal_error(lpp, -1, -1, "failed to read input file\n");
	}

	FILE* output_file; 

	if (lpp->output_file_name)
	{
		output_file = fopen(lpp->output_file_name, "w");
		if (!output_file)
		{
			perror("fopen");
			fatal_error(lpp, -1, -1, "failed to open output file '%s'\n", lpp->output_file_name);
		}
	}
	else
		output_file = stdout;


	lpp_lexer_init(lpp, &lpp->lexer, input_buffer);
	lpp_lexer_run(&lpp->lexer);
	
	

	for (s32 i = 0; i < lpp->lexer.token_count; i++)
	{
		fprintf(stdout, "%i: %s\n", i, token_kind_strings[lpp->lexer.tokens[i].kind]);
	}
}
