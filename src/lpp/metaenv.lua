-- [[
--     Definition of the meta-environment. 
--     This is the environment that a lua metaprogram will execute in. 
-- ]]

-- The metaenvironment table.
-- All things in this table are accessible as 
-- global things from within the metaprogram.
local M = {}

-- Shallow copy the global table into the meta env. 
for k,v in pairs(_G) do
    M[k] = v
end 

-- Table storing functions for interacting 
-- with lpp from within a metaprogram
-- eg. 
-- __LPP.next_token()
M.__LPP = {}


-- [[
--     Vars used to track internal 
--     state of the metaprogram.
-- ]]


-- The buffer in which the final C program is 
-- constructed.
-- TODO(sushi) make this a luajit string buffer 
local result = ""


-- [[
--     __LPP functions
-- ]]


local function append_result(s)
	if type(s) ~= "string" then 
		error("append_result() got "..type(s).." instead of a string!", 3)
	end

	result = result .. s
end


-- [[
--    Metaenvironment functions.
--    These are directly accessible by the metaprogram
--    and should not be overwritten by it.
-- ]]


M.
__C = function(str)
    if str ~= nil then
        append_result(str)
    end
end

M.
__VAL = function(x)
	append_result(tostring(x))
end

M. 
__FINAL = function()
	return result
end

M.
__MACRO = function(result)
    if result ~= nil then
        if type(result) ~= "string" then
            error("Lua macro did not result in a string, got: "..type(result), 2)
        end
        return result
    end
end

return M
