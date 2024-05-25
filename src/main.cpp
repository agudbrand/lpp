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

	auto out = scoped(fs::File::stdout());
	if (isnil(out))
		return 1;

	str args[] = { "google.com"_str };

	fs::File curlout;
	Process::Stream streams[3] = {{}, {true, &curlout}, {}};

	auto proc = Process::spawn("curl"_str, {args, 1}, streams);

	for (;;)
	{
		u8 buffer[128];
		u64 len = curlout.read({buffer, 128});
		if (len)
			io::format(&out, str::from(buffer, len));
		proc.checkStatus();
		if (proc.terminated)
		{
			io::formatv(&out, "curl terminated with exit code ", proc.exit_code, "\n");
			break;
		}
	}

	Logger logger;
	logger.init("lpp"_str, Logger::Verbosity::Trace);

	{
		using enum Log::Dest::Flag;
		iro::log.newDestination("stdout"_str, &fs::stdout, 
				AllowColor |
				ShowCategoryName |
				ShowVerbosity |
				TrackLongestName |
				PadVerbosity);
	}

	auto testlpp = scoped(fs::File::from("tests/lppclang.lpp"_str, fs::OpenFlag::Read));
	if (isnil(testlpp))
		return 1;

	Lpp lpp = {};
	if (!lpp.init(logger.verbosity))
		return 1;
	// defer { lpp.deinit(); };

	io::Memory mp;
	mp.open();

	Metaprogram m = lpp.createMetaprogram("tests/lppclang.lpp"_str, &testlpp, &mp);
	if (!m)
		return 1;

	fs::stdout.write({mp.buffer, mp.len});

	auto outfile = fs::File::from("temp/out"_str, fs::OpenFlag::Write | fs::OpenFlag::Create);
	if (isnil(outfile))
		return 1;

	io::formatv(&outfile, "test\n");

	if (!lpp.runMetaprogram(m, &mp, &outfile))
		return 1;

	return 0;
}
