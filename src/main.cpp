#include "lpp.h"

#include "iro/fs/fs.h"
#include "iro/common.h"
#include "iro/logger.h"

#define DEFINE_GDB_PY_SCRIPT(script_name) \
  asm("\
.pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n\
.byte 1 /* Python */\n\
.asciz \"" script_name "\"\n\
.popsection \n\
");

DEFINE_GDB_PY_SCRIPT("lpp-gdb.py");

int main(int argc, const char** argv) 
{
  iro::log.init();
  defer { iro::log.deinit(); };

  Logger logger;
  logger.init("lpp"_str, Logger::Verbosity::Trace);

  {
    using enum Log::Dest::Flag;
    Log::Dest::Flags flags;

    if (fs::stdout.isatty())
    {
      flags = 
          AllowColor
        | ShowCategoryName
        | ShowVerbosity
        | TrackLongestName
        | PadVerbosity
        | PrefixNewlines;
    }
    else
    {
      flags = 
          ShowCategoryName
        | ShowVerbosity
        | PrefixNewlines;
    }

    iro::log.newDestination("stdout"_str, &fs::stdout, flags);
  }

  Lpp lpp = {}; 
  if (!lpp.init())
    return 1;
  defer { lpp.deinit(); };

  if (!lpp.processArgv(argc, argv))
    return 1;

  if (!lpp.run())
    return 1;

  return 0;
}
