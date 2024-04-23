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

#include "generated/cliargs.h"

int main(int argc, char** argv) 
{
	log.init();
	defer { log.deinit(); };

	Logger logger;
	logger.init("lpp"_str, Logger::Verbosity::Trace);

	io::FileDescriptor out;
	out.open(1);
	defer { out.close(); };

	log.new_destination("stdout"_str, &out, Log::Dest::Flags::all());

	logger.log(Logger::Verbosity::Trace, "hello\nthere\nman!");

	io::FileDescriptor testcpp;
	if (!testcpp.open("test.lpp"_str, io::Flag::Readable))
		return 1;
	defer { testcpp.close(); };

	Lpp lpp;
	if (!lpp.init(&testcpp, &out, logger.verbosity))
		return 1;

	if (!lpp.run())
		return 1;

	return 0;
}
