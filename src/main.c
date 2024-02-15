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

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef u8 b8; // boolean type

typedef enum
{
	PartType_Lua,
	PartType_C,
} PartType;

// a part of the given source file, which may either
// be some C or Lua code.
typedef struct 
{
	PartType type;
	u8* start;
	u32 len;
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

	// singluar lua state
	lua_State* L;
} lpp;

#define TRACE(fmt, ...)                        \
	do {                                       \
		fprintf(stdout, "\e[36mtrace\e[0m: "); \
		fprintf(stdout, fmt, ##__VA_ARGS__);   \
	} while(0); 


// @str
// basic string view
typedef struct str
{	
	u8* s;
	s32 count;
} str;

b8 isempty(str s)
{
	while (s.count--) if (!isspace(s.s[s.count])) return 0;
	return 1;
}

// @memory
// memory wrappers in case i ever want to alter
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

dstr dstr_init(const char* s)
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
		out.s = 0;
	}

	return out;
}

void dstr_clear(dstr* s)
{
	memory_free(s->s);
	s->count = 0;
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

void initialize_lua()
{
	lpp.L = lua_open();
	luaL_openlibs(lpp.L);
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
	p->start = part.s;
	p->len = part.count;
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

void parse_lua()
{
	str s;

	advance(); // from the $
	while (!at('\n') && isspace(current()))
		advance();
	
	if (at('\n'))
	{
		warning("empty lua line\n");
		return;
	}

	switch (current())
	{
		case '(':
		{
			advance();
			s.s = lpp.cursor;
			while (!at(')') && !eof()) advance();
			if (eof()) 
				fatal_error("unexpected eof while parsing parenthesized lua expression (eg. $(...))\n");
			s.count = lpp.cursor - s.s;
			advance();
		}		
		break;
		default:
		{
			s.s = lpp.cursor;
			while (!at('\n') && !eof()) advance();
			s.count = lpp.cursor - s.s;
		}
	}

	push_part(PartType_Lua, s);
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
			default:
				if (at(LSYMBOL))
					goto end;
				else
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
		if (at(LSYMBOL))
			parse_lua();
		else
			parse_c();
	}
	return 1;
}

void print_parts()
{
	for (u32 i = 0; i < lpp.n_parts; i++)
	{
		Part* p = lpp.parts + i;
		fprintf(stdout, "%s: %.*s\n", (p->type == PartType_C ? "C" : "Lua"), p->len, p->start);
	}
}

int main(int argc, char** argv) 
{
	lpp.use_color = 1;
	process_args(argc, argv);
	read_file();
	process_file();
	print_parts();

	initialize_lua();


	return 0;
}
