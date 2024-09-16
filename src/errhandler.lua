local lpp = require "lpp"
local ffi = require "ffi"
local C = ffi.C

return function(errmsg)
  if "table" == type(errmsg) and errmsg.handled then return end

  local mpctx = lpp.context

  local stack = {}
  local fidx = 2
  while true do
    local info = debug.getinfo(fidx)
    if not info then break end
    if info.what ~= "C" then
      table.insert(stack, 1,
      {
        src = info.source,
        line = info.currentline,
        name = info.name,
        func = info.func,
      })
    end
    fidx = fidx + 1
  end

  for _,s in ipairs(stack) do
    if s.name then
      io.write(s.src, ":", s.line, ": in ", s.name, ":\n")
    else
      io.write(s.src, ":", s.line, ":\n")
    end
  end
  io.write(errmsg, "\n")
end
