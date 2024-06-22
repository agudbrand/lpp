#include "vec.h"

$$$
require("lppclang").init(lpp, "../lppclang/build/debug/liblppclang.so")

local printFields = function()
  local section = lpp.getSectionAfterMacro()

  local ctx = lpp.clang.createContext()

  ctx:addIncludeDir("tests/clang/with-includes")
  ctx:parseString(lpp.getOutputSoFar())

  local diter = ctx:parseString(section:getString()):getDeclIter()
  local struct = diter:next()
  local members = struct:getDeclIter()
  while true do
    local member = members:next()

    if not member then
      break
    end

    if not member:isField() then
      goto continue
    end

    io.write(member:name(), ": ", member:type():getTypeName(), "\n")

    ::continue::
  end
end

$$$

@printFields
struct Player
{
  vec3 pos;
};
