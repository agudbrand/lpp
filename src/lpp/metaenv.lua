
local ffi = require "ffi"
ffi.cdef [[
    typedef struct TokenReturn {
        char* raw_str;
        int   raw_len;

		TokenKind kind;

		int line;
		int column;
		char* file_name;
    } TokenReturn;

	TokenReturn get_token(void* ctx, int idx);
]]
local C = ffi.C

local M = {}   

local __LPP = {}
M.__LPP = __LPP

do -- shallow copy the global table into the meta env
	for k,v in pairs(_G) do
		M[k] = v
	end
end

local result = ""
local tok_idx = 0
local token_array = {}

__LPP.
current_token = function()
	local tok = C.get_token(__LPP.context, tok_idx)
	local out = {}
	out.file_name = ffi.string(tok.file_name)
	out.kind = tok.kind
	out.line = tok.line
	out.column = tok.column
	out.raw = ffi.string(tok.raw_str, tok.raw_len)
	return out
end

__LPP.
next_token = function()
	tok_idx = tok_idx + 1
	return __LPP.current_token()
end

local function append_result(s)
	if type(s) ~= "string" then
		error("append_result() got "..type(s).." instead of a string!", 3)
	end

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

M.
__TOKENCONTEXT = function(idx)
    tok_idx = idx
end 

M.
__MACRO = function(result)
	if not result then
		error("Lua macro did not result in a string, got: "..type(result), 2)
	end
	return result
end

return M
