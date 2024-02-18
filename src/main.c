#include "common.h"

#include "lpp.h"

#include "stdio.h"
#include "stdlib.h"
#include "stdarg.h"

void arg_error(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stdout, "error: ");
	vfprintf(stdout, fmt, args);
	exit(1);
}

int main(int argc, char** argv) 
{
	lppContext lpp = {}; 

	for (int i = 1; i < argc; i++) 
	{
		char* arg = argv[i];
		if (arg[0] == '-')
		{
			if (arg[1] == '-')
				arg_error("no options are supported yet!\n");

			if (arg[1] == 'o')
			{
				i += 1;
				if (i == argc)
					arg_error("expected a path after '-o'\n");
				
				arg = argv[i];
				
				lpp.output_file_name = (u8*)arg;
			}
		}
		else
		{
			if (lpp.input_file_name) // TODO(sushi) support multiple input files
				arg_error("an input file has already been specified (%s); only one is supported for now!\n", lpp.input_file_name);

			// this must be an input file  
			lpp.input_file_name = (u8*)arg;
		}
	}

	if (!lpp.input_file_name)
		arg_error("no input file\n");

	lpp_run(&lpp);

	return 0;
}
