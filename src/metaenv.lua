local M = {}

do -- shallow copy the global table into the meta env
	for k,v in pairs(_G) do
		M[k] = v
	end
end

local result = ""

local function append_result(s)
	result = result .. s
end

M.
__C = function(str)
	append_result(str)
end

M.
__VAL = function(x)
	append_result(tostring(x))
end

M.
__FINAL = function()
	return result
end

return M
