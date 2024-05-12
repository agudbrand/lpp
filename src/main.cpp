#include "common.h"

#include "lpp.h"

#include "stdlib.h"
#include "stdarg.h"

#include "io/io.h"
#include "fs/fs.h"
#include "scoped.h"

#include "logger.h"

#define DEFINE_GDB_PY_SCRIPT(script_name) \
  asm("\
.pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n\
.byte 1 /* Python */\n\
.asciz \"" script_name "\"\n\
.popsection \n\
");

DEFINE_GDB_PY_SCRIPT("lpp-gdb.py");

#include "generated/cliargs.h"

int main(int argc, char** argv) 
{
	iro::log.init();
	defer { iro::log.deinit(); };

	Logger logger;
	logger.init("lpp"_str, Logger::Verbosity::Error);

	auto out = scoped(fs::File::fromStdout());
	if (out == nil)
		return 1;

	{
		using enum Log::Dest::Flag;
		iro::log.newDestination("stdout"_str, &out, 
			Log::Dest::Flags::from(
				AllowColor,
				ShowCategoryName, 
				ShowVerbosity));
	}

	auto testlpp = scoped(fs::File::from("old.lpp"_str, fs::OpenFlag::Read));
	if (testlpp == nil)
		return 1;

	Lpp lpp = {};
	if (!lpp.init(logger.verbosity))
		return 1;

	io::Memory mp;
	mp.open();

	Metaprogram m = lpp.createMetaprogram("old.lpp"_str, &testlpp, &mp);
	if (!m)
		return 1;

	out.write({mp.buffer, mp.len});

	auto outfile = fs::File::from("temp/out"_str, fs::OpenFlags::from(fs::OpenFlag::Write, fs::OpenFlag::Create));
	if (outfile == nil)
		return 1;

	io::formatv(&outfile, "test\n");

	if (!lpp.runMetaprogram(m, &mp, &outfile))
		return 1;

	lpp.deinit();

	return 0;
}
