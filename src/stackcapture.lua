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

	local fidx = 2 + offset
	while true do
		local info = debug.getinfo(fidx)
		if not info then break end
		if info.what ~= "C" then
			stack:push(
			{
				src = info.source,
				line = info.currentline,
				name = info.name
			})
		end
		fidx = fidx + 1
	end

	return stack
end
