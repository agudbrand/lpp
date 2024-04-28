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
	lpp::log.init();
	defer { lpp::log.deinit(); };

	Logger logger;
	logger.init("lpp"_str, Logger::Verbosity::Warn);

	io::FileDescriptor out;
	out.open(1);
	defer { out.close(); };

	lpp::log.new_destination("stdout"_str, &out, Log::Dest::Flags::all());

	io::FileDescriptor testlpp;
	if (!testlpp.open("test.lpp"_str, io::Flag::Readable))
		return 1;
	defer { testlpp.close(); };

	Lpp lpp = {};
	if (!lpp.init(logger.verbosity))
		return 1;

	io::Memory mp;
	mp.open();

	Metaprogram m = lpp.create_metaprogram("test.lpp"_str, &testlpp, &mp);
	if (!m)
		return 1;

	out.write({mp.buffer, mp.len});

	if (!lpp.run_metaprogram(m, &mp, &out))
		return 1;

	lpp.deinit();

	return 0;
}
