-- [[
--     Definition of the meta-environment. 
--     This is the environment that a lua metaprogram will execute in. 
-- ]]

-- Define our interface to C.
local ffi = require "ffi"
-- TODO(sushi) replace a lot of this by preprocessing common.h 
--             and just loading it into ffi from the C code
ffi.cdef [[
    enum TokenKind;

    typedef struct TokenReturn {
        char* raw_str;
        int   raw_len;

		TokenKind kind;

		int line;
		int column;
		char* file_name;
    } TokenReturn;

	TokenReturn get_token(void* ctx, int idx);

    TokenKind string_to_tokenkind(u64 hash)

    typedef struct str {
        char* s;
        int32_t len; 
    } str;

    uint64_t hash_string(str s)
]]
local C = ffi.C

local strtype = ffi.typeof("str")

ffi.metatype("TokenKind", {
    __eq = function(lhs, rhs)
        if type(rhs) == "string" then
            local kind = C.string_to_tokenkind(C.hash_string(strtype(rhs, #rhs)))
            return lhs == kind
        elseif type(rhs) == "number" then
            return lhs == rhs
        else
            error("== with a Token only supports strings and numbers!", 2)
        end
    end
})

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
-- The index of the current token at any point
-- in the metaprogram. This is 
local tok_idx = 0


-- [[
--     __LPP functions
-- ]]


M.__LPP.
current_token = function()
	local tok = C.get_token(M.__LPP.context, tok_idx)
	local out = {}
	out.file_name = ffi.string(tok.file_name)
	out.kind = tok.kind
	out.line = tok.line
	out.column = tok.column
	out.raw = ffi.string(tok.raw_str, tok.raw_len)
	return out
end

M.__LPP.
next_token = function()
    while true do
        tok_idx = tok_idx + 1
        local tok = M.__LPP.current_token()
        if tok.kind ~= "comment" and tok.kind ~= "whitespace" then
            return tok
        elseif tok.kind == 0 then
            return nil
        end
    end
end

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
