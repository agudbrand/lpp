#include "common.h"

#include "lpp.h"

#include "stdio.h"
#include "stdlib.h"
#include "stdarg.h"

#include "io.h"
#include "json/types.h"
#include "json/parser.h"
#include "uri.h"

#include "logger.h"

#define DEFINE_GDB_PY_SCRIPT(script_name) \
  asm("\
.pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n\
.byte 1 /* Python */\n\
.asciz \"" script_name "\"\n\
.popsection \n\
");

DEFINE_GDB_PY_SCRIPT("lpp-gdb.py");

#include "unistd.h"

int main(int argc, char** argv) 
{
	log.init();
	defer { log.deinit(); };

	io::FileDescriptor out;
	out.open(1);

	Log::Dest::Flags flags = Log::Dest::Flags::all();
	flags.unset(Log::Dest::Flag::ShowDateTime);

	log.new_destination("stdout"_str, &out, flags);

	json::JSON json;
	json::Parser parser;

	io::FileDescriptor jsonfile;
	if (!jsonfile.open("test.json"_str, io::Flag::Readable))
	{
		printf("failed to open test.json\n");
		return 1;
	}

	if (!parser.init(&jsonfile, &json, "test.json"_str, Logger::Verbosity::Trace))
		return 1;

	if (!parser.start())
		return 1;

	io::formatv(&out, json, "\n");

	io::FileDescriptor testfile;
	if (!testfile.open("temp/test.c"_str, io::Flag::Readable))
	{
		printf("failed to open test file\n");
		return 1;
	}

	io::Memory result;

	Lpp lpp;

	if (!lpp.init(&testfile, &result, Logger::Verbosity::Trace))
		return 1;
	
	if (!lpp.run())
		return 1;

	out.write(result.as_str());



	return 0;
}
