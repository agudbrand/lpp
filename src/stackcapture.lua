local List = require "list"

---@class Call
--- The identifier of the source file this call was made in.
---@field src string
--- The name of the function being called, if available.
---@field name string?
--- The line number in src where this call was made
---@field line number 

---@return List
return function(offset)
  local stack = List()
  offset = offset or 0
  offset = offset + 2

  local stack_pos = 1

  local fidx = offset
  local cidx = 0
  while true do
    local info = debug.getinfo(fidx + cidx)
    if not info then break end
    -- print(info.source)
    if info.what ~= "C" then
      local tbl =
      {
        src = info.source,
        line = info.currentline,
        name = info.name,
        pos = stack_pos,
        func = info.func,
      }
      stack:push(tbl)
      fidx = fidx + 1
    else
      cidx = cidx + 1
    end

    stack_pos = stack_pos + 1
  end

  return stack, stack_pos
end
