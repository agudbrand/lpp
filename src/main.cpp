#include "iro/common.h"

#include "lpp.h"

#include "stdlib.h"
#include "stdarg.h"

#include "iro/io/io.h"
#include "iro/fs/fs.h"
#include "iro/traits/scoped.h"

#include "iro/logger.h"

#include "string.h"

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

// TODO(sushi) this is currently shared between here and lake.cpp, it should be moved to an arg helper
//             in iro.
struct ArgIter
{
  const char** argv = nullptr;
  u64 argc = 0;
  u64 idx = 0;

  str current = nil;

  ArgIter(const char** argv, u32 argc) : argv(argv), argc(argc) 
  {  
    idx = 1;
    next();
  }

  void next()
  {
    if (idx == argc)
    {
      current = nil;
      return;
    }

    current = { (u8*)argv[idx++] };
    current.len = strlen((char*)current.bytes);
  }
};

b8 processArgv(int argc, const char** argv, str* out_file)
{
  for (ArgIter iter(argv, argc); notnil(iter.current); iter.next())
  {
    str arg = iter.current;
    if (arg.bytes[0] == '-')
    {
      io::formatv(&fs::stdout, 
        "error: arguments other than the processed file are not supported yet.");
      return false;
    }

    *out_file = arg;
    break;
  }
  return true;
}

int main(int argc, const char** argv) 
{
  iro::log.init();
  defer { iro::log.deinit(); };

  Logger logger;
  logger.init("lpp"_str, Logger::Verbosity::Trace);

  {
    using enum Log::Dest::Flag;
    iro::log.newDestination("stdout"_str, &fs::stdout, 
          AllowColor 
        | ShowCategoryName
        | ShowVerbosity
        | TrackLongestName
        | PadVerbosity
        | PrefixNewlines);
  }

  // return 0;

  // TODO(sushi) actual cli args in lpp, especially when we get to supporting an lsp 
  str file = nil;
  if (!processArgv(argc, argv, &file))
    return 1;

  if (isnil(file))
  {
    FATAL("no input file\n");
    return 1;
  }

  Lpp lpp = {}; 
  if (!lpp.init())
    return 1;
  // defer { lpp.deinit(); };
  
  auto filehandle = scoped(fs::File::from(file, fs::OpenFlag::Read));
  if (isnil(filehandle))
    return 1;

  io::Memory mp;
  mp.open();

  io::IO* out = nullptr;
  
  auto outfile = fs::File::from("temp/out"_str, fs::OpenFlag::Write | fs::OpenFlag::Create | fs::OpenFlag::Truncate);
  auto outclose = deferWithCancel { outfile.close(); };
  if (notnil(outfile))
  {
    out = &outfile;
  }
  else
  {
    outclose.cancel();
    out = &fs::stdout;
  }

  if (!lpp.processStream(file, &filehandle, &outfile))
    return 1;

  return 0;
}
