#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdint.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "ctype.h"

// the symbol used to start lua code
#define LSYMBOL '$'
// the macro symbol
#define MSYMBOL '@'

#define TRACE(fmt, ...)                        \
	do {                                       \
		fprintf(stdout, "\e[36mtrace\e[0m: "); \
		fprintf(stdout, fmt, ##__VA_ARGS__);   \
	} while(0); 

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef u8 b8; // boolean type

// @str
// basic string view
typedef struct str
{	
	u8* s;
	s32 count;
} str;

// @memory
// memory wrappers in case I ever want to alter
// this behavior later or add memory tracking
void* memory_reallocate(void* ptr, u64 n_bytes)
{
	return realloc(ptr, n_bytes);
}

void memory_free(void* ptr)
{
	free(ptr);
}

void memory_copy(void* dst, void* src, s32 bytes)
{
	memcpy(dst, src, bytes);
}

// @dstr
// a basic dynamic str type
typedef struct dstr
{
	u8* s;
	s32 count;
	s32 space;
} dstr;

dstr dstr_create(const char* s)
{
	dstr out = {};

	if (s)
	{
		out.count = strlen(s);
		out.s = memory_reallocate(0, out.count);
		memory_copy(out.s, (char*)s, out.count);
	}
	else
	{
		out.space = 4;
		out.count = 0;
		out.s = memory_reallocate(0, out.space);
	}

	return out;
}

void dstr_destroy(dstr* s)
{
	memory_free(s->s);
	s->s = 0;
	s->count = 0;
}

void dstr_grow_if_needed(dstr* x, s32 bytes)
{
	if (x->count + bytes <= x->space)
		return;

	TRACE("grow by %i\n", bytes);

	while(x->space < x->count + bytes) x->space *= 2;
	x->s = memory_reallocate(x->s, x->space);
}

void dstr_push_cstr(dstr* x, const char* str)
{
	u32 len = strlen(str);
	dstr_grow_if_needed(x, len);
	memory_copy(x->s + x->count, (void*)str, len);
	x->count += len;
}

void dstr_push_str(dstr* x, str s)
{
	dstr_grow_if_needed(x, s.count);
	memory_copy(x->s + x->count, s.s, s.count);
	x->count += s.count;
}

// pushs the STRING reprentation of 
// 'c' to the dstr, not the character c
void dstr_push_u8(dstr* x, u8 c)
{
	char buf[3];
	s32 n = snprintf(buf, 3, "%u", c);
	dstr_grow_if_needed(x, n);
	memory_copy(x->s + x->count, buf, n);
	x->count += n;
}

void dstr_push_char(dstr* x, u8 c)
{
	dstr_grow_if_needed(x, 1);
	x->s[x->count] = c;
	x->count += 1;
}

typedef enum
{
	PartType_LuaLine,
	PartType_LuaValue,
	PartType_LuaMacro,
	PartType_LuaMacroStringed,
	PartType_LuaBlock,
	PartType_C,
} PartType;

// a part of the given source file, which may either
// be some C or Lua code.
typedef struct 
{
	PartType type;
	str s;
} Part;

struct // globals 
{
	// the file we're going to process
	char* input_file_str;

	// where the processed output will go
	char* output_file_str;

	FILE* input_file;
	FILE* output_file;

	u8* input_buffer;

	u8* cursor;

	u8* c_start;

	Part* parts;
	u32 n_parts;

	u32 line;
	u32 column;

	// whether to use color in stdout
	b8 use_color;

	dstr metaprogram;

	// singluar lua state
	lua_State* L;
} lpp;

b8 isempty(str s)
{
	while (s.count--) if (!isspace(s.s[s.count])) return 0;
	return 1;
}



// print the given message and then exit
void fatal_error(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stdout,  "\e[31mfatal error\e[0m: ");
	vfprintf(stdout, fmt, args);
	exit(1);
}

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


void putcolor(Color color)
{
	if (lpp.use_color)
		fprintf(stdout, "\e[%im", color);
}

void error(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	putcolor(Color_Blue);
	fprintf(stdout, "%s", lpp.input_file_str);
	putcolor(Color_RESET);
	fprintf(stdout, ":%u:%u: ", lpp.line, lpp.column);
	putcolor(Color_Red);
	fprintf(stdout, "error");
	putcolor(Color_RESET);
	fprintf(stdout, ": ");
	vfprintf(stdout, fmt, args);
}

void warning(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	putcolor(Color_Blue);
	fprintf(stdout, "%s", lpp.input_file_str);
	putcolor(Color_RESET);
	fprintf(stdout, ":%u:%u: ", lpp.line, lpp.column);
	putcolor(Color_Yellow);
	fprintf(stdout, "warning");
	putcolor(Color_RESET);
	fprintf(stdout, ": ");
	vfprintf(stdout, fmt, args);
}

// process the arguments given to the program
// and handle opening files and stuff
void process_args(int argc, char** argv)
{
	for (int i = 1; i < argc; i++) 
	{
		char* arg = argv[i];
		if (arg[0] == '-')
		{
			if (arg[1] == '-')
				fatal_error("no options are supported yet!\n");

			if (arg[1] == 'o')
			{
				i += 1;
				if (i == argc)
					fatal_error("expected a path after '-o'\n");
				
				arg = argv[i];
				
				lpp.output_file_str = arg;
				lpp.output_file = fopen(arg, "w");

				if (!lpp.output_file)
				{
					perror("fopen");
					fatal_error("could not open given output file\n");
				}

				TRACE("opened output file '%s'\n", lpp.output_file_str);
			}
		}
		else
		{
			if (lpp.input_file_str)
				fatal_error("an input file has already been specified (%s); only one is supported!\n", lpp.input_file);

			// this must be an input file 
			lpp.input_file_str = arg;
			lpp.input_file = fopen(arg, "r");

			if (!lpp.input_file)
			{
				perror("fopen");
				fatal_error("could not open input file '%s'\n", arg);
			}

			TRACE("opened input file '%s'\n", lpp.input_file_str);
		}
	}

	if (!lpp.input_file)
		fatal_error("no input file\n");
}



// runs the given string as lua code.
// returns false if the code fails.
b8 runlua(const char* s)
{
	if (luaL_loadstring(lpp.L, s) || lua_pcall(lpp.L, 0, 0, 0))
	{
		fprintf(stdout, "lua error from runlua():\n%s\n", lua_tostring(lpp.L, -1));
		lua_pop(lpp.L, 1);
		return 0;
	}
	return 1;
}

// opens the input file 
b8 read_file()
{
	fseek(lpp.input_file, 0, SEEK_END);
	u32 file_size = ftell(lpp.input_file);
	fseek(lpp.input_file, 0, SEEK_SET);

	lpp.input_buffer = malloc(file_size);

	u32 bytes_read = fread(lpp.input_buffer, file_size, 1, lpp.input_file);

	if (ferror(lpp.input_file))
	{
		perror("fread");
		fatal_error("failed to read input file\n");
	}

	lpp.line = lpp.column = 1;

	TRACE("read file\n");

	return 1;	
}

void push_part(PartType type, str part)
{
	lpp.parts = memory_reallocate(lpp.parts, sizeof(Part) * (lpp.n_parts + 1));
	Part* p = lpp.parts + lpp.n_parts;
	p->type = type;
	p->s = part;
	lpp.n_parts += 1;
}

// returns whether the cursor is at eof
b8 eof() { return *lpp.cursor == 0; }

u8 current() { return *lpp.cursor; }
u8 next() { return *(lpp.cursor + 1); }

// returns whether the cursor is at the given character
b8 at(u8 c) { return current() == c; }
b8 next_at(u8 c) { return next() == c; }

b8 at_quote() { return at('\'') || at('"'); }
b8 at_comment() { return  at('/') && (next_at('/') || next_at('*')); }

void advance() 
{ 
	if (at('\n'))
	{
		lpp.line += 1;
		lpp.column = 1;
	}  
	else 
		lpp.column += 1;

	lpp.cursor++; 
}
void advancen(u32 n) { for (u32 i = 0; i < n; i++) advance(); }

void skip_line()
{
	while (!at('\n') && !eof()) advance();
	if (!eof()) advance();
}

void skip_quotes()
{
	switch (*lpp.cursor)
	{
		case '\'': 
			advance();
			while (!at('\'') && !eof()) 
				advance();
		break;
		case '"':
			advance();
			while (!at('"') && !eof()) 
				advance();
		break;
	}

	if (!eof()) advance();
}

void skip_comment()
{
	if (next_at('/'))
		skip_line();
	else 
	{
		while ((!at('*') || !next_at('/')) && !eof()) advance();
		if (!eof()) advancen(2);
	}
}

void skip_whitespace()
{
	while(isspace(current())) advance();
}

void parse_macro()
{
	str s;

	advance();
	skip_whitespace();
	if (!isalpha(current()))
	{
		error("expected an identifier (one of a function)\n");
		return;
	}

	s.s = lpp.cursor;

	while(!at('(') && !at('"') && !eof()) advance();

	if (eof())
	{
		error("expected '(' after macro identifier\n");
		return;
	}

	if (at('"'))
	{
		// single string argument
		advance();
		while(!at('"') && !eof()) advance();
		if (eof())
		{
			error("expected '\"' to end stringed macro\n");
			return;
		}
		advance();
		s.count = lpp.cursor - s.s;
		push_part(PartType_LuaMacroStringed, s);
		advance();
		return;
	}

	advance();

	u32 nesting = 1;

	while (!eof())
	{
		if (at('('))
			nesting += 1;
		else if (at(')'))
		{
			nesting -= 1;
			if (!nesting) break;
		}
		advance();
	}

	if (eof())
	{
		error("expected a ')' to end macro arguments\n");
		return;
	}

	advance();

	s.count = lpp.cursor - s.s;

	// fprintf(stdout, "%.*s\n", s.count, s.s);

	push_part(PartType_LuaMacro, s);
}

void parse_lua()
{
	str s;

	if (at(MSYMBOL))
	{
		parse_macro();
		return;
	}

	advance(); // from the $
	while (!at('\n') && isspace(current()))
		advance();
	
	if (at('\n'))
	{
		warning("empty lua line\n");
		return;
	}

	PartType type;

	switch (current())
	{
		case '(':
		{
			advance();
			s.s = lpp.cursor;
			b8 multiline = 0;
			u32 nesting = 1;
			while (!eof()) 
			{
				if (at('\n'))
					multiline = 1;
				else if (at('('))
					nesting += 1;
				else if (at(')'))
				{
					nesting -= 1;
					if (!nesting) break;
				}
				advance();
			}
			if (eof()) 
				fatal_error("unexpected eof while parsing parenthesized lua expression (eg. $(...))\n");
			s.count = lpp.cursor - s.s;
			advance();
			type = multiline ? PartType_LuaLine : PartType_LuaValue;
		}		
		break;
		default:
		{
			s.s = lpp.cursor;
			while (!at('\n') && !eof()) advance();
			s.count = lpp.cursor - s.s;
			type = PartType_LuaLine;
		}
	}

	push_part(type, s);
}

void parse_c()
{
	str s;
	s.s = lpp.cursor;
	
	while (!eof())	
	{
		skip_whitespace();

		switch (current())
		{
			case '/':
				if (next_at('/') || next_at('*'))
					skip_comment();
			break;
			case '\'':
			case '"':
				skip_quotes();
			break;
			case LSYMBOL:
			case MSYMBOL:
				goto end;
			break;
			default:
				advance();
		}
	}

end: 
	s.count = lpp.cursor - s.s;
	
	if (!isempty(s))
		push_part(PartType_C, s);
}

int process_file()
{
	lpp.cursor = 
	lpp.c_start = lpp.input_buffer;

	while (!eof())
	{
		switch (current())
		{
			case LSYMBOL:
			case MSYMBOL:
				parse_lua();
			break;
			default:
				parse_c();
		}
	}
	return 1;
}

void print_parts()
{
	for (u32 i = 0; i < lpp.n_parts; i++)
	{
		Part* p = lpp.parts + i;
		fprintf(stdout, "%s: %.*s\n", (p->type == PartType_C ? "C" : "Lua"), p->s.count, p->s.s);
	}
}

void build_metaprogram()
{
	lpp.metaprogram = dstr_create(0);
	dstr* mp = &lpp.metaprogram;

	for (u32 i = 0; i < lpp.n_parts; i++)
	{
		Part* p = lpp.parts + i;
		
		switch (p->type)
		{
			case PartType_LuaLine:
			case PartType_LuaBlock:
				dstr_push_str(mp, p->s);
				dstr_push_char(mp, '\n');
			break;
			case PartType_LuaValue:
				dstr_push_cstr(mp, "__VAL(");
				dstr_push_str(mp, p->s);
				dstr_push_cstr(mp, ")\n");
			break;
			case PartType_LuaMacro:
			{
				dstr_push_cstr(mp, "__C(");

				lpp.cursor = p->s.s;

				str name;
				name.s = lpp.cursor;

				while (!at('(') && !isspace(current())) advance();

				name.count = lpp.cursor - name.s;

				dstr_push_str(mp, name);
				dstr_push_char(mp, '(');

				if (!at('(')) while (!at('(')) advance();
				advance();

				str arg;

				u32 nesting = 1;
				while (!eof())
				{
					skip_whitespace();
					arg.s = lpp.cursor;
					
					while (!at(','))
					{
						if (at(')')) 
						{
							nesting -= 1;
							if (!nesting) break;
						}
						else if (at('(')) nesting += 1;
						advance();
					}

					arg.count = lpp.cursor - arg.s;
					dstr_push_char(mp, '"');
					dstr_push_str(mp, arg);
					dstr_push_char(mp, '"');

					if (at(')')) break;

					dstr_push_cstr(mp, ",");
					advance();
				}

				dstr_push_cstr(mp, "))\n");
			}
			break;
			case PartType_LuaMacroStringed:
			{
				dstr_push_cstr(mp, "__C(");

				lpp.cursor = p->s.s;

				str name;
				name.s = lpp.cursor;

				while (!at('"') && !isspace(current())) advance();

				name.count = lpp.cursor - name.s;

				dstr_push_str(mp, name);
				dstr_push_char(mp, '(');

				str arg;

				if (!at('"')) skip_whitespace();
				arg.s = lpp.cursor;
				advance();
				while (!at('"')) advance();
				advance();

				arg.count = lpp.cursor - arg.s;
				dstr_push_str(mp, arg);
				dstr_push_cstr(mp, "))\n");
			}
			break;
			case PartType_C:
				dstr_push_cstr(mp, "__C(\"");
				// need to sanitize
				for (u32 i = 0; i < p->s.count; i++) 
				{
					u8 c = p->s.s[i];
					if (iscntrl(c))
					{
						dstr_push_char(mp, '\\');
						dstr_push_u8(mp, c);
					}
					else if (c == '"')
					{
						dstr_push_char(mp, '\\');
						dstr_push_u8(mp, '"');
					}
					else if (c == '\\')
					{
						dstr_push_char(mp, '\\');
						dstr_push_char(mp, '\\');
					}
					else 
						dstr_push_char(mp, c);
				}
				dstr_push_cstr(mp, "\")\n");
			break;
		}
	}

	dstr_push_cstr(mp, "io.write(__FINAL())\n");

	fprintf(stdout, "%.*s", mp->count, mp->s);
}

void initialize_lua()
{
	lpp.L = lua_open();
	luaL_openlibs(lpp.L);
}

b8 luacall(u32 n_returns)
{
	if (lua_pcall(lpp.L, 0, n_returns, 0))
	{
		error("%s\n", lua_tostring(lpp.L, -1));
		lua_pop(lpp.L, 1);
		return 0;
	}

	return 1;
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


const char* reader(lua_State* L, void* data, size_t* size)
{
	dstr* s = data;
	if (s->s)
	{
		*size = s->count;
		dstr_destroy(s);
		return (char*)s->s;
	}
	else 
		return 0;
}

void load_metaprogram()
{
	if (luaL_loadbuffer(lpp.L, (char*)lpp.metaprogram.s, lpp.metaprogram.count, lpp.input_file_str))
	{
		fprintf(stdout, "%s\n", lua_tostring(lpp.L, -1));
		lua_pop(lpp.L, 1);
		return;
	}
#define METAPROG_IDX 1

	// TODO(sushi) replace with C array 
	//             xxd -i src/metaenv.lua
	if (luaL_loadfile(lpp.L, "src/metaenv.lua"))
	{
		fprintf(stdout, "%s\n", lua_tostring(lpp.L, -1));
		lua_pop(lpp.L, 1);
		return;
	}

	if (!luacall(1))
		fatal_error("metaenv failed to run\n");

	if (!lua_setfenv(lpp.L, METAPROG_IDX))
		fatal_error("failed to set environment of metaprogram");
}

void run_metaprogram()
{
	luacall(0);	
}

int main(int argc, char** argv) 
{
	lpp.use_color = 1;
	process_args(argc, argv);
	read_file();
	process_file();
	print_parts();
	build_metaprogram();
	initialize_lua();
	load_metaprogram();
	run_metaprogram();
	


	return 0;
}
