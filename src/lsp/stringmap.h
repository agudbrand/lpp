/*
 *  Type that represents arbitrary mapping from strings to something else
 *  in lsp json stuff.
 */

#ifndef _lpp_lsp_stringmap_h
#define _lpp_lsp_stringmap_h

#include "iro/common.h"
#include "iro/unicode.h"
#include "iro/containers/pool.h"
#include "iro/containers/avl.h"

namespace lsp
{

using namespace iro;

/* ============================================================================
 */
template<typename T>
struct StringMapElement
{
  str key;
  u64 hash;
  T   value;

  StringMapElement<T>() { }
};

/* --------------------------------------------------------------------------
 */
template<typename T>
u64 hashStringMapElement(const StringMapElement<T>* x)
{
  return x->hash;
}

/* ============================================================================
 */
template<typename T>
struct StringMap
{
  using Element = StringMapElement<T>;

  AVL<Element, hashStringMapElement<T>> map;
  Pool<Element> pool;

  StringMap() { }

  /* --------------------------------------------------------------------------
   */
  b8 init(mem::Allocator* allocator = &mem::stl_allocator)
  {
    if (!map.init(allocator))
      return false;
    if (!pool.init(allocator))
      return false;
    return true;
  }

  /* --------------------------------------------------------------------------
   */
  void deinit()
  {
    map.deinit();
    for (Element& elem : map)
      pool.allocator->free(elem.key.bytes);
    pool.deinit();
  }

  /* --------------------------------------------------------------------------
   */
  T* add(str key)
  {
    u64 hash = key.hash();
    if (Element* v = map.find(hash))
      return &v->value;

    Element* nu = pool.add();
    nu->key = key.allocateCopy();
    nu->hash = hash;
    nu->value = {};
    
    return &nu->value;
  }

  /* --------------------------------------------------------------------------
   */
  T* find(str key)
  {
    if (Element* v = map.find(key.hash()))
      return &v->value;
    return nullptr;
  }
};

}

#endif

