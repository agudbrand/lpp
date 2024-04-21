#include "common.h"

#include "lpp.h"

#include "stdio.h"
#include "stdlib.h"
#include "stdarg.h"

#include "json/parser.h"

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
	json::Parser p;
	
	FILE* f = fopen("test.json", "r");

	str buffer = {};

	fseek(f, 0, SEEK_END);
	buffer.len = ftell(f);
	fseek(f, 0, SEEK_SET);

	buffer.bytes = (u8*)mem.allocate(buffer.len);

	if (!fread(buffer.bytes, buffer.len, 1, f))
	{
		ERROR("failed to read test.json\n");
		return 1;
	}

	json::JSON json;

	p.init(buffer, &json, "test.json"_str);

	if (!p.start())
		return 1;

	p.deinit();

	json.pretty_print();

	return 0;

	Lpp lpp = {};

	lpp.input_file_name = "temp/test.c"_str;

	lpp.run();

	return 0;
}
