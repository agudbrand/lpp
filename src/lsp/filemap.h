/*
 *  Mapping from an open file to the buffer provided by the client.
 *
 *  Filenames and content are cached internally. Not sure yet if this is a 
 *  good idea.
 */

#ifndef _lpp_lsp_filemap_h
#define _lpp_lsp_filemap_h

#include "iro/common.h"
#include "iro/unicode.h"
#include "iro/containers/pool.h"
#include "iro/containers/avl.h"
#include "iro/memory/bump.h"

namespace lpp::lsp
{

using namespace iro;

/* ============================================================================
 */
struct File
{
  String filename;
  u64 hash; // of filename
  String contents;
};

/* --------------------------------------------------------------------------
 */
static u64 hashFileMapElement(const File* x)
{
  return x->hash;
}

/* ============================================================================
 */
struct FileMap
{
  AVL<File, hashFileMapElement> map;
  Pool<File> pool;

  mem::LenientBump filename_allocator;

  /* --------------------------------------------------------------------------
   */
  b8 init(mem::Allocator* allocator = &mem::stl_allocator)
  {
    if (!map.init(allocator)) return false;
    if (!pool.init(allocator)) return false;
    if (!filename_allocator.init()) return false;
    return true;
  }

  /* --------------------------------------------------------------------------
   */
  void deinit()
  {
    map.deinit();
    pool.deinit();
    filename_allocator.deinit();
  }

  /* --------------------------------------------------------------------------
   */
  File* getOrCreateFile(String filename)
  {
    u64 hash = filename.hash();
    
    if (File* existing = map.find(hash))
      return existing;

    File* new_file = pool.add();
    if (!new_file)
      return nullptr;
    map.insert(new_file);

    new_file->filename = filename.allocateCopy(&filename_allocator);
    new_file->hash = hash;
    new_file->contents = nil;

    return new_file;
  }

  /* --------------------------------------------------------------------------
   */
  File* getFile(String filename)
  {
    return map.find(filename.hash());
  }

  b8 updateFileContents(File* file, String contents)
  {
    assert(file);

    if (notnil(file->contents))
    {
      mem::stl_allocator.free(file->contents.bytes);
    }

    file->contents = contents.allocateCopy();

    return true;
  }
};

}

#endif
