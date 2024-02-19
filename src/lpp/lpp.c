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
		case LUA_TLIGHTUSERDATA:
		case LUA_TUSERDATA: printf("%p", lua_touserdata(L, idx)); return;
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

#include "generated/metaenv.h"

typedef struct TokenReturn {
	char* raw_str;
	int   raw_len;

	TokenKind kind;

	int line;
	int column;
	char* file_name;
} TokenReturn;

TokenReturn get_token(lppContext* lpp, int idx)
{
	TokenReturn out = {};

	Token t = lpp->lexer.tokens[idx];
	out.raw_str = t.raw.s;
	out.raw_len = t.raw.len;
	out.file_name = lpp->input_file_name;
	out.kind = t.kind;
	out.line = t.line;
	out.column = t.column;

	return out;
}

static void build_metac(dstr* x, str s)
{
	dstr_push_cstr(x, "__C(\"");
	for (u32 i = 0; i < s.len; i++)
	{
		u8 c = s.s[i];
		if (iscntrl(c))
		{
			dstr_push_char(x, '\\');
			dstr_push_u8(x, c);
		}
		else if (c == '"')
		{
			dstr_push_char(x, '\\');
			dstr_push_u8(x, '"');
		}
		else if (c == '\\')
		{
			dstr_push_char(x, '\\');
			dstr_push_char(x, '\\');
		}
		else
			dstr_push_char(x, c);
	}
	dstr_push_cstr(x, "\")\n");
}

static str consolidate_c_tokens(Token* start, Token* end)
{
	if (start == end) 
		return start->raw;
	
	str out;
	out.s = start->raw.s;
	out.len = end->raw.s - out.s + end->raw.len;
	return out;
}

static void build_metalua(lppContext* lpp, dstr* x, Token* lua_token)
{
	dstr_push_cstr(x, "__TOKENCONTEXT(");
	dstr_push_s64(x, lua_token - lpp->lexer.tokens);
	dstr_push_cstr(x, ")\n");

	switch (lua_token->kind)
	{
		case tok_lpp_lua_inline: {
			dstr_push_cstr(x, "__VAL(");
			dstr_push_str(x, lua_token->raw);
			dstr_push_cstr(x, ")\n");
		} break;

		case tok_lpp_lua_line:
		case tok_lpp_lua_block: {
			dstr_push_str(x, lua_token->raw);
			dstr_push_char(x, '\n');
		} break;

		case tok_lpp_lua_macro: {
			dstr_push_cstr(x, "__C(");
			dstr_push_cstr(x, "__MACRO(");
			dstr_push_str(x, lua_token->raw);
			dstr_push_char(x, '(');
			lua_token += 1;
			for (;;)
			{
				dstr_push_char(x, '"');
				dstr_push_str(x, lua_token->raw);
				dstr_push_char(x, '"');
				if ((lua_token + 1)->kind != tok_lpp_lua_macro_argument)
					break;
				dstr_push_char(x, ',');
				lua_token += 1;
			}
			dstr_push_cstr(x, ")))\n");
		} break;

		default: 
			fatal_error(lpp, lua_token->line, lua_token->column, "INTERNAL ERROR: non-lua token passed to build_metalua()\n");
	}
}

static void build_metaprogram(lppContext* lpp)
{
	lpp->metaprogram = dstr_create(0);
	dstr* mp = &lpp->metaprogram;

	Lexer* l = &lpp->lexer;
	
	Token* tokens = l->tokens;
	u64* lua_tokens = l->lua_tokens;

	// handle first edge case outside of loop
	u64 first_idx = lua_tokens[0];
	Token* first_lua_token = tokens + first_idx;

	if (first_idx)
	{
		str c_str = consolidate_c_tokens(tokens, tokens + first_idx - 1);
		build_metac(mp, c_str);
	}

	build_metalua(lpp, mp, first_lua_token);

	for (s32 i = 1; i < l->lua_token_count; i++)
	{
		u64 last_lua_token_idx = lua_tokens[i-1]; 
		u64 lua_token_idx = lua_tokens[i];
		Token* lua_token = tokens + lua_token_idx;

		if (lua_token->kind == tok_lpp_lua_macro_argument) 
			continue; // these are handled by the macro token, so just skip them

		if (lua_token_idx != last_lua_token_idx + 1)
		{
			str c_str = consolidate_c_tokens(tokens + last_lua_token_idx + 1, lua_token - 1);
			build_metac(mp, c_str);
		}

		build_metalua(lpp, mp, lua_token);
	}

	s32 remaining_c_tokens = l->token_count - lua_tokens[l->lua_token_count-1];

	if (remaining_c_tokens)
	{
		str c_str = consolidate_c_tokens(tokens + lua_tokens[l->lua_token_count-1]+1, tokens + l->token_count-1);
		build_metac(mp, c_str);
	}

	dstr_push_cstr(mp, "return __FINAL()\n");
}

static void load_metaprogram(lppContext* lpp)
{
	lua_State* L = lpp->L;
	if (luaL_loadbuffer(L, (char*)lpp->metaprogram.s, lpp->metaprogram.len, lpp->input_file_name))
	{
		fprintf(stdout, "%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return;
	}

	if (luaL_loadbuffer(L, metaenv_bytes, metaenv_byte_count, "meta environment"))
	{
		fprintf(stdout, "Failed to load metaenv:\n%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return;
	}

	if (lua_pcall(L, 0, 1, 0))
	{
		fprintf(stdout, "Failed to run metaenv:\n%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return;
	}

	// set the context pointer 
	lua_pushstring(L, "__LPP"); 
	lua_gettable(L, -2);
	lua_pushstring(L, "context");
	lua_pushlightuserdata(L, lpp);
	lua_settable(L, -3);
	lua_pop(L, 1);

	if (!lua_setfenv(L, 1))
	{
		fprintf(stdout, "Failed to set environment of metaprogram:\n%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return;
	}
}

static void run_metaprogram(lppContext* lpp)
{
	lua_State* L = lpp->L;

	if (lua_pcall(L, 0, 1, 0))
	{
		fprintf(stdout, "Metaprogram failed:\n%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return;
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
		fprintf(stdout, "%i: %s: %.*s\n", i, token_kind_strings[lpp->lexer.tokens[i].kind], lpp->lexer.tokens[i].raw.len, lpp->lexer.tokens[i].raw.s);
	}

	build_metaprogram(lpp);

	initialize_lua(lpp);
	load_metaprogram(lpp);
		
	// fprintf(stdout, "%.*s\n", lpp->metaprogram.len, lpp->metaprogram.s);

	run_metaprogram(lpp);

	fprintf(output_file, "%s\n", lua_tostring(lpp->L, -1));
}
