#include "iro/common.h"

#include "lpp.h"

#include "stdlib.h"
#include "stdarg.h"

#include "iro/io/io.h"
#include "iro/fs/fs.h"
#include "iro/traits/scoped.h"

#include "iro/logger.h"

#include "iro/process.h"

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
	logger.init("lpp"_str, Logger::Verbosity::Trace);

	{
		using enum Log::Dest::Flag;
		iro::log.newDestination("stdout"_str, &fs::stdout, 
				AllowColor |
				ShowCategoryName |
				ShowVerbosity |
				TrackLongestName |
				PadVerbosity |
				PrefixNewlines);
	}

	Lpp lpp = {}; 
	if (!lpp.init())
		return 1;
	// defer { lpp.deinit(); };
	
	auto cwd = scoped(fs::Path::cwd());
	auto testdir = scoped(fs::Path::from("tests/import"_str));

	testdir.chdir();

	str testpath = "import.lpp"_str;

	auto testlpp = scoped(fs::File::from(testpath, fs::OpenFlag::Read));
	if (isnil(testlpp))
		return 1;

	io::Memory mp;
	mp.open();

	Metaprogram m = lpp.createMetaprogram(testpath, &testlpp, &mp);
	if (!m)
		return 1;

	INFO(mp.asStr());

	io::Memory result;
	result.open();

	if (!lpp.runMetaprogram(m, &mp, &result))
		return 1;

	cwd.chdir();

	auto outfile = fs::File::from("temp/out"_str, fs::OpenFlag::Write | fs::OpenFlag::Create | fs::OpenFlag::Truncate);
	if (notnil(outfile))
	{
		outfile.write(result.asStr());
		outfile.close();
	}

	return 0;
}
