-- [[
--     Definition of the meta-environment. 
--     This is the environment that a lua metaprogram will execute in. 
-- ]]

local ffi = require "ffi"
ffi.cdef [[

	typedef uint8_t  u8;
	typedef uint16_t u16;
	typedef uint32_t u32;
	typedef uint64_t u64;
	typedef int8_t   s8;
	typedef int16_t  s16;
	typedef int32_t  s32;
	typedef int64_t  s64;
	typedef float    f32;
	typedef double   f64;
	typedef u8       b8; // booean type

	typedef struct str
	{
		u8* s;
		s32 len;
	} str;

	str get_token_indentation(void* lpp, s32);
]]
local C = ffi.C


-- The metaenvironment table.
-- All things in this table are accessible as 
-- global things from within the metaprogram.
local M = {}

-- Shallow copy the global table into the meta env. 
for k,v in pairs(_G) do
    M[k] = v
end 


-- [[
--     Vars used to track internal 
--     state of the metaprogram.
-- ]]


-- The buffer in which the final C program is 
-- constructed.
-- TODO(sushi) make this a luajit string buffer 
local result = ""

local macro_token_index = nil

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


M.__SUBJECT = function(str, token)
    if str ~= nil then
        append_result(str)
    end
end

M.__VAL = function(x, token)
	if x == nil then
		error("inline lua expression resulted in nil", 2)
	end
	local s = tostring(x)
	if s == nil then
		error("result of inline lua expression was not convertible to a string", 2)
	end
	append_result(tostring(x))
end

M.__SET_MACRO_TOKEN_INDEX = function(idx)
	macro_token_index = idx
end

M.__MACRO = function(result, token)
    if result ~= nil then
        if type(result) ~= "string" then
            error("lua macro did not result in a string, got: "..type(result), 2)
        end
        return result
    end
end

M.__FINAL = function()
	return result
end


-- [[
--    User facing lpp api, accessed via the lpp var anywhere in the metaenvironment
-- ]]


M.lpp = {}
local lpp = M.lpp

-- returns the indentation preceeding the current token
lpp.indentation = function()
	local i = C.get_token_indentation(lpp.handle, macro_token_index)
	return ffi.string(i.s, i.len)
end

return M, lpp
