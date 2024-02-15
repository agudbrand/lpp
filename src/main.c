#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

// the symbol used to start lua code
#define LSYMBOL '$'

struct // globals 
{
	// the file we're going to process
	char* input_file_str;

	// where the processed output will go
	char* output_file_str;

	FILE* input_file;
	FILE* output_file;

	// singluar lua state
	lua_State* L;
} g;

#define TRACE(fmt, ...)                        \
	do {                                       \
		fprintf(stdout, "\e[36mtrace\e[0m: "); \
		fprintf(stdout, fmt, ##__VA_ARGS__);   \
	} while(0); 

// print the given message and then exit
void fatal_error(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stdout,  "\e[31mfatal error\e[0m: ");
	vfprintf(stdout, fmt, args);
	exit(1);
}

// process the arguments given to the program
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

				g.output_file_str = arg;
				g.output_file = fopen(arg, "w");

				if (!g.output_file)
				{
					perror("fopen");
					fatal_error("could not open given output file\n");
				}

				TRACE("opened output file %s\n", g.output_file_str);
			}
		}
		else
		{
			if (g.input_file_str)
				fatal_error("an input file has already been specified (%s); only one is supported!\n", g.input_file);

			// this must be an input file 
			g.input_file_str = arg;
			g.input_file = fopen(arg, "r");

			if (!g.input_file)
			{
				perror("fopen");
				fatal_error("could not open input file '%s'\n", arg);
			}
		}
	}

	if (!g.input_file)
		fatal_error("no input file\n");
}

void initialize_lua()
{
	g.L = lua_open();
	luaL_openlibs(g.L);
}

// runs the given string as lua code.
// returns false if the code fails.
int runlua(const char* s)
{
	if (luaL_loadstring(g.L, s) || lua_pcall(g.L, 0, 0, 0))
	{
		fprintf(stdout, "lua error from runlua():\n%s\n", lua_tostring(g.L, -1));
		lua_pop(g.L, 1);
		return 0;
	}
	return 1;
}

// opens the entire given file 
int open_file()
{
	
}

int process_file()
{
	
}

int main(int argc, char** argv) 
{
	process_args(argc, argv);
		
	initialize_lua();

	runlua("a = 0");

	runlua("print(a)");

	return 0;
}
