--- definition of import to be required by other files
local lpp = require "lpp"

return function(path)
	local result = lpp.processFile(path)

	local expansion = lpp.MacroExpansion.new()
	expansion:pushBack(
		lpp.MacroPart.new(
			path, 0, 0, result))

	return expansion
end
