#include "lpp.h"

#include "iro/logger.h"
#include "iro/fs/fs.h"

#include "assert.h"

#include "metaprogram.h"

extern "C"
{
#include "lua.h"
}

static Logger logger = Logger::create("lpp"_str, Logger::Verbosity::Debug);

const char* lpp_metaenv_stack = "__lpp_metaenv_stack";

/* ------------------------------------------------------------------------------------------------
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
  if (!lua.dofile("src/cdefs.lua"))
  {
    ERROR("failed to load luajit ffi\n");
    return false;
  }

  if (!lua.require("lpp"_str))
    return false;

  // give lpp module a handle to us
  lua.pushstring("handle"_str);
  lua.pushlightuserdata(this);
  lua.settable(lua.gettop()-2);
  lua.pop();

    return true;
}

/* ------------------------------------------------------------------------------------------------
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

/* ------------------------------------------------------------------------------------------------
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

/* ================================================================================================ lua C API
 *  C api exposed to the internal lpp module 
 */
extern "C"
{

struct MetaprogramBuffer
{
  io::Memory* memhandle;
  u64         memsize;
};

/* ------------------------------------------------------------------------------------------------
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

/* ------------------------------------------------------------------------------------------------
 *  Copies into 'outbuf' the metaprogram stored in 'mpbuf'. If the metaprogram cannot be retrieved 
 *  for some reason then 'outbuf' should be null. Frees the metaprogram memory in anycase.
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


