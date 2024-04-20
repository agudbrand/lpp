#include "common.h"

#include "lpp.h"

#include "stdio.h"
#include "stdlib.h"
#include "stdarg.h"

#define DEFINE_GDB_PY_SCRIPT(script_name) \
  asm("\
.pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n\
.byte 1 /* Python */\n\
.asciz \"" script_name "\"\n\
.popsection \n\
");

DEFINE_GDB_PY_SCRIPT("lpp-gdb.py");

int main(int argc, char** argv) 
{
	Lpp lpp = {};

	lpp.input_file_name = "temp/test.c"_str;

	lpp.run();

	return 0;
}
