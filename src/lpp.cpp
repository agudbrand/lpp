#include "lpp.h"

#include "stdio.h"

#include "iro/logger.h"
#include "iro/fs/fs.h"
#include "iro/argiter.h"
#include "iro/platform.h"

// Disabled for now since im giving up working on this and it 
// actually causes clang to crash when compiling sometimes lol.
// #include "lsp/server.h"

#include "assert.h"

#include "metaprogram.h"

#include "signal.h"
#include "unistd.h"
#include "sys/types.h"

extern "C"
{
#include "lua.h"

int lua__processFile(lua_State* L);
int lua__getFileFullPathIfExists(lua_State* L);
int lua__debugBreak(lua_State* L);
}

namespace lpp
{

static Logger logger = 
  Logger::create("lpp"_str, 
#if LPP_DEBUG
    Logger::Verbosity::Debug);
#else
    Logger::Verbosity::Notice);
#endif

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

#define addGlobalCFunc(name) \
  lua.pushcfunction(name); \
  lua.setglobal(STRINGIZE(name));

  addGlobalCFunc(lua__processFile);
  addGlobalCFunc(lua__getFileFullPathIfExists);
  addGlobalCFunc(lua__debugBreak);

#undef addGlobalCFunc

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
  assert(!"lsp has been disabled for now");
  return true;
  // DEBUG("running lsp\n");

  // lsp::Server server;

  // if (!server.init(lpp))
  //   return false;
  // defer { server.deinit(); };

  // return server.loop();
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

  if (generate_depfile)
  {
    if (!processStream(input, &inf, nullptr))
      return false;

    if (!lua.require("lpp"_str))
      return false;
    const s32 I_lpp = lua.gettop();

    lua.pushstring("generateDepFile"_str);
    lua.gettable(I_lpp);

    if (!lua.pcall(0, 1))
      return false;
    
    str result = lua.tostring();

    if (notnil(depfile_output))
    {
      auto outf = File::from(depfile_output,
            OpenFlag::Create
          | OpenFlag::Write
          | OpenFlag::Truncate);

      outf.write(result);
      outf.close();
    }
    else
    {
      fs::stdout.write(result);
    }
  }
  else
  {
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
  }

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

  lua.pushstring("addIncludeDir"_str);
  lua.gettable(I_lpp);
  const s32 I_addIncludeDir = lua.gettop();

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
      break;
    case "print-meta"_hashed:
      print_meta = true;
      break;
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
        return FATAL("output already specified as '", output, "'\n");
      iter.next();
      if (isnil(iter.current))
        return FATAL("expected an output path after '-o'\n");
      output = iter.current;
      break;

    // A 'require' directory. Will append the given path + ?.lua to 
    // package.path.
    case "R"_hashed:
      iter.next();
      if (isnil(iter.current))
        return FATAL("expected a path after '-R'\n");
      lua.getglobal("package");
      lua.pushstring("path"_str);
      lua.gettable(-2);
      lua.pushstring(";"_str);
      lua.pushstring(iter.current);
      lua.pushstring("/?.lua"_str);
      lua.concat(4);
      lua.pushstring("path"_str);
      lua.pushvalue(-2);
      lua.settable(-4);
      lua.pop(2);
      break;

    // An include directory, which will be searched when using lpp.include.
    case "I"_hashed:
      iter.next();
      if (isnil(iter.current))
        return FATAL("expected a path after '-I'\n");
      lua.pushvalue(I_addIncludeDir);
      lua.pushstring(iter.current);
      if (!lua.pcall(1))
        return false;
      break;

    // Outputs a lake dependency file to the path given.
    // Suppresses any normal output.
    case "D"_hashed:
      iter.next();
      if (isnil(iter.current))
        return FATAL("expected an output path for '-D'\n");
      generate_depfile = true;
      depfile_output = iter.current;
      lua.pushstring("generating_dep_file"_str);
      lua.pushboolean(true);
      lua.settable(I_lpp);
      break;

    case "t"_hashed:
      logger.verbosity = Logger::Verbosity::Trace;
      break;

    case "om"_hashed:
      iter.next();
      if (isnil(iter.current))
        return FATAL("expected a path to output metafile to");
      output_metafile = true;
      metafile_output = iter.current;
      break;

    default:
      passthroughToLua();
    }

    return true;
  };

  for (;notnil(iter.current); iter.next())
  {
    str arg = iter.current;
    
    if (arg.ptr[0] == '-')
    {
      if (arg.ptr[1] == '-')
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

  Metaprogram* prev = (metaprograms.isEmpty()? nullptr : &metaprograms.tail());
  Metaprogram* metaprog = metaprograms.pushTail()->data;
  if (!metaprog->init(this, instream, source, dest, prev))
    return false;
  defer { metaprog->deinit(); };

  if (!metaprog->run())
    return false;

  if (outstream)
    outstream->write(metaprog->output->cache.asStr());
  
  return true;
}

/* ============================================================================
 *  C api exposed to the internal lpp module 
 */
extern "C"
{

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
int lua__processFile(lua_State* L)
{
  auto lua = LuaState::fromExistingState(L);
  Lpp* lpp = lua.tolightuserdata<Lpp>(1);
  auto reqpath = lua.tostring(2);
  auto path = fs::Path::from(reqpath);

  if (!path.makeAbsolute())
  {
    FATAL("failed to make path ", path, " absolute\n");
    _exit(1);
  }

  if (!lua.require("lpp"_str))
    return 0;
  const s32 I_lpp = lua.gettop();

  lua.pushstring("addDependency"_str);
  lua.gettable(I_lpp);

  lua.pushstring(path.buffer.asStr());
  if (!lua.pcall(1))
  {
    ERROR("failed to add dependency on ", path, "\n");
    ERROR(lua.tostring(), "\n");
    return 0;
  }

  auto f = fs::File::from(path, fs::OpenFlag::Read);
  if (isnil(f))
    return {};
  defer { f.close(); };

  io::Memory mem;
  mem.open();
  defer { mem.close(); };

  if (!lpp->processStream(reqpath, &f, &mem))
    return 0;

  lua.pushstring(mem.asStr());
  return 1;
}

/* ----------------------------------------------------------------------------
 */
int lua__fileExists(lua_State* L)
{
  auto lua = LuaState::fromExistingState(L);
  str path = lua.tostring(1);
  lua.pushboolean(
      fs::Path::exists(path) &&
      fs::Path::isRegularFile(path));
  return 1;
}

/* ----------------------------------------------------------------------------
 */
int lua__getFileFullPathIfExists(lua_State* L)
{
  using namespace fs;
  auto lua = LuaState::fromExistingState(L);
  auto pathstr = lua.tostring(1);
  if (Path::exists(pathstr) &&
      Path::isRegularFile(pathstr))
  {
    auto path = fs::Path::from(lua.tostring(1));
    defer { path.destroy(); };
    if (!path.makeAbsolute())
    {
      FATAL("failed to make path ", path, "absolute!\n");
      _exit(1);
    }
    lua.pushstring(path.buffer.asStr());
    return 1;
  }
  return 0;
}

/* ----------------------------------------------------------------------------
 */
int lua__debugBreak(lua_State* L)
{
  platform::debugBreak();
  return 0;
}

}

}
