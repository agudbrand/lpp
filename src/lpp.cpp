#include "lpp.h"

#include "iro/logger.h"
#include "iro/fs/fs.h"
#include "iro/argiter.h"

#include "lsp/server.h"

#include "assert.h"

#include "metaprogram.h"

#include "signal.h"
#include "unistd.h"
#include "sys/types.h"

extern "C"
{
#include "lua.h"
}

static Logger logger = Logger::create("lpp"_str, Logger::Verbosity::Debug);

const char* lpp_metaenv_stack = "__lpp_metaenv_stack";

/* ----------------------------------------------------------------------------
 */
b8 Lpp::init()
{
  INFO("init\n");

  DEBUG("creating lua state\n");
  lua.init();

  DEBUG("creating metaenv stack\n");
  lua.newtable();
  lua.setglobal(lpp_metaenv_stack);

  DEBUG("creating pools\n");
  sources.init();
  metaprograms.init();

  DEBUG("loading luajit ffi\n");
  if (!lua.require("cdefs"_str))
  {
    ERROR("failed to load luajit ffi\n");
    return false;
  }

  input = output = nil;

  DEBUG("loading lpp lua module\n");
  if (!lua.require("lpp"_str))
    return false;

  // give lpp module a handle to us
  lua.pushstring("handle"_str);
  lua.pushlightuserdata(this);
  lua.settable(lua.gettop()-2);
  lua.pop();

  DEBUG("done initializing\n");
  initialized = true;

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Lpp::deinit()
{
  lua.deinit();

  for (auto& source : sources)
    source.deinit();
  sources.deinit();

  for (auto& metaprogram : metaprograms)
    metaprogram.deinit();
  metaprograms.deinit();

  *this = {};
}

/* ----------------------------------------------------------------------------
 */
static b8 runLsp(Lpp* lpp)
{
  DEBUG("running lsp\n");

  DEBUG("pid is: ", getpid(), "\n");
  raise(SIGSTOP);

  lsp::Server server;

  if (!server.init(lpp))
    return false;
  defer { server.deinit(); };

  return server.loop();
}

/* ----------------------------------------------------------------------------
 */
b8 Lpp::run()
{
  DEBUG("run\n");

  using namespace fs;

  if (lsp)
    return runLsp(this);

  if (isnil(input))
  {
    FATAL("no input file specified\n");
    return false;
  }

  File inf = File::from(input, OpenFlag::Read);
  if (isnil(inf))
  {
    FATAL("failed to open input file at path '", input, "'\n");
    return false;
  }
  defer { inf.close(); };

  File outf;
  if (isnil(output))
  {
    outf = fs::stdout;
  }
  else
  {
    outf = File::from(output, 
          OpenFlag::Create
        | OpenFlag::Write
        | OpenFlag::Truncate);
  }
  if (isnil(outf))
  {
    FATAL("failed to open output file at path '", output, "'\n");
    return false;
  }
  defer { outf.close(); };
  
  if (!processStream(input, &inf, &outf))
    return false;

  return true;
}

/* ----------------------------------------------------------------------------
 */
static void makeLspTempOut()
{
  using namespace fs;
  using enum OpenFlag;
  using enum Log::Dest::Flag;

  auto f = mem::stl_allocator.construct<File>();
  *f = 
    File::from("lpplsp.log"_str, 
        Write
      | Truncate
      | Create);
  if (isnil(*f))
  {
    ERROR("failed to open lpplsp.log\n");
    return;
  }
  log.newDestination("lpplsp.log"_str, f,
        ShowCategoryName
      | ShowVerbosity);
}

/* ----------------------------------------------------------------------------
 */
b8 Lpp::processArgv(int argc, const char** argv)
{
  DEBUG("processing argv\n");

  // Handle cli args that lpp recognizes and store the rest in a lua table 
  // to be handled by the metaprogram.
  if (!lua.require("lpp"_str))
    return false;
  const s32 I_lpp = lua.gettop();

  lua.newtable();
  const s32 I_argv_table = lua.gettop();

  lua.pushstring("argv"_str);
  lua.pushvalue(I_argv_table);
  lua.settable(I_lpp);

  lua.getglobal("table");
  const s32 I_table = lua.gettop();

  lua.pushstring("insert"_str);
  lua.gettable(I_table);
  const s32 I_table_insert = lua.gettop();

  ArgIter iter(argv, argc);

  auto passthroughToLua = [=, &iter, this]()
  {
    lua.pushvalue(I_table_insert);
    lua.pushvalue(I_argv_table);
    lua.pushstring(iter.current);
    lua.pcall(2, 0);
  };

  auto handleDoubleDash = [=, &iter, this]() -> b8
  {
    str arg = iter.current.sub(2);
    switch (arg.hash())
    {
    case "lsp"_hashed:
      lsp = true;
      makeLspTempOut();
    default:
      passthroughToLua();
    }
    
    return true;
  };

  auto handleSingleDash = [=, &iter, this]() -> b8
  {
    str arg = iter.current.sub(1);
    switch(arg.hash())
    {

    case "o"_hashed:
      if (notnil(output))
      {
        FATAL("output already specified as '", output, "'\n");
        return false;
      }
      iter.next();
      if (isnil(iter.current))
      {
        FATAL("expected an output path after '-o'\n");
        return false;
      }
      output = iter.current;
      break;

    default:
      passthroughToLua();
    }

    return true;
  };

  for (;notnil(iter.current); iter.next())
  {
    str arg = iter.current;
    
    if (arg.bytes[0] == '-')
    {
      if (arg.bytes[1] == '-')
      {
        if (!handleDoubleDash())
          return false;
      }
      else
      {
        if (!handleSingleDash())
          return false;
      }
    }
    else
    {
      if (isnil(input))
      {
        input = iter.current;
      }
      else
      {
        passthroughToLua();
      }
    }
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Lpp::processStream(str name, io::IO* instream, io::IO* outstream)
{
  DEBUG("creating metaprogram from input stream '", name, "'\n");

  Source* source = sources.pushTail()->data;
  if (!source->init(name))
    return false;
  defer { source->deinit(); };

  Source* dest = sources.pushTail()->data;
  if (!dest->init("dest"_str)) // TODO(sushi) dont use 'dest' here.
    return false;
  defer { dest->deinit(); };

  Metaprogram* metaprog = metaprograms.pushTail()->data;
  if (!metaprog->init(this, instream, source, dest))
    return false;
  defer { metaprog->deinit(); };

  if (!metaprog->run())
    return false;

  outstream->write(metaprog->output->cache.asStr());
  
  return true;
}

/* ============================================================================
 *  C api exposed to the internal lpp module 
 */
extern "C"
{

struct MetaprogramBuffer
{
  io::Memory* memhandle;
  u64         memsize;
};

/* ----------------------------------------------------------------------------
 *  Wrapper around an lpp context's create_metaprogram() and run_metaprogram(). This creates a 
 *  metaprogram, executes it, and then returns a handle to the memory buffer its stored in along 
 *  with the size needed to store it in memory. This must be passed back into 
 *  get_metaprogram_result() along with a luajit string buffer with enough space to fit the 
 *  program.
 *
 *  TODO(sushi) this seems really inefficient, but I think it should work fine as a first pass
 *              and can be made more efficient/elegant later.
 *              We could maybe create an io::IO that wraps a luajit string buffer and feed 
 *              this information directly to it, but that's for another time.
 *  TODO(sushi) we can maybe add a bool to run_metaprogram to instruct it to leave the metaprogram 
 *              on the lua stack instead of popping it when its finished. I don't know if doing 
 *              this is fine during a luajit ffi call, though, and I can't seem to find anyone 
 *              on the internet or in the luajit docs mentioning doing something similar.
 *              -- OK NOW this is not as possible because i've changed how buffers work,
 *                 buffer data is no longer stored in lua, we store it in Metaenvironment::Sections.
 *                 So this might just be how this has to be for now until I think of a better solution.
 *                 This might not even be that much better than making a lua string and passing it 
 *                 directly!!!!
 *  TODO(sushi) this is so dumb, just make this a normal lua C function and pass the result back
 *              as a string.
 */ 
LPP_LUAJIT_FFI_FUNC
MetaprogramBuffer processFile(Lpp* lpp, str path)
{
  auto f = fs::File::from(path, fs::OpenFlag::Read);
  if (isnil(f))
    return {};
  defer { f.close(); };

  auto result = mem::stl_allocator.construct<io::Memory>();
  result->open();
  defer { result->close(); };

  if (!lpp->processStream(path, &f, result))
    return {};

  MetaprogramBuffer out;
  out.memhandle = result;
  out.memsize = result->len;
  return out;
}

/* ----------------------------------------------------------------------------
 *  Copies into 'outbuf' the metaprogram stored in 'mpbuf'. If the metaprogram 
 *  cannot be retrieved for some reason then 'outbuf' should be null. Frees the 
 *  metaprogram memory in anycase.
 */
LPP_LUAJIT_FFI_FUNC
void getMetaprogramResult(MetaprogramBuffer mpbuf, void* outbuf)
{
  if (outbuf)
  {
    mpbuf.memhandle->read({(u8*)outbuf, mpbuf.memsize});
  }

  mpbuf.memhandle->close();
}

}


