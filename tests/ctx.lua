local List = require "list"
local lpp = require "lpp"
require "lppclang" .init("../lppclang/build/debug/liblppclang.so")

string.startsWith = function(self, s)
  return self:sub(1,#s) == s
end

local args = List{}

for arg in os.getenv("NIX_CFLAGS_COMPILE"):gmatch("%S+") do
  args:push(arg)
end

for _,v in ipairs(lpp.argv) do
  if v:startsWith "--cargs" then
    local cargs = v:sub(#"--cargs="+1)
    for carg in cargs:gmatch("[^,]+") do
      args:push(carg)
    end
  end
end

return lpp.clang.createContext(args)
