-- [[
--     Definition of the meta-environment. 
--     This is the environment that a lua metaprogram will execute in. 
-- ]]


local ffi = require "ffi"
local C = ffi.C

local strtype = ffi.typeof("str")
local make_str = function(s)
	return strtype(s, #s)
end

local buffer = require "string.buffer"
local co = require "coroutine"

-- The metaenvironment table.
-- All things in this table are accessible as 
-- global things from within the metaprogram.
local M = {}

-- Shallow copy the global table into the meta env. 
--for k,v in pairs(_G) do
--    M[k] = v
--end
M.__index = _G

setmetatable(M, M)

M.__metaenv =
{
	queue = {},
	phase_one={}
}
local menv = M.__metaenv


-- [[
--     Vars used to track internal 
--     state of the metaprogram.
-- ]]


-- The buffer in which the final C program is 
-- constructed.
-- TODO(sushi) make this a luajit string buffer 
local result = ""

-- queue of document strings and macros to invoke after
-- the first translation phase

local macro_token_index = nil


-- [[
--    Metaenvironment functions.
--    These are directly accessible by the metaprogram
--    and should not be overwritten by it.
-- ]]


local append_document = function(str)
	local current = menv.queue[#menv.queue]
	if not current or
		   current.type == "macro" then
		current = {type="document",buf=buffer.new()}
		table.insert(menv.queue,current)
	end
	current.buf:put(str)
end

menv.doc = function(str)
	append_document(str)
end

menv.val = function(x)
	if x == nil then
		error("inline lua expression resulted in nil", 2)
	end
	local s = tostring(x)
	if not s then
		error("result of inline lua expression is not convertible to a string", 2)
	end
	append_document(s)
end

menv.macro = function(offset, m, ...)
	if not m then
		error("macro identifier is nil", 2)
	end
	table.insert(menv.queue, {type = "macro", offset = offset, macro = m, args = {...}})
end

-- TODO(sushi) make this more graceful later
local current_sec = nil

menv.final = function()
	for k,v in pairs(M) do
		print(k, v)
	end

	local buf = buffer.new()
	for _,sec in ipairs(menv.queue) do
		if sec.type == "document" then
			buf:put(sec.buf)
		elseif sec.type =="macro" then
			buf:put(sec.macro(unpack(sec.args)))
		end
	end
	return buf:get()
end


-- [[
--    User facing lpp api, accessed via the lpp var anywhere in the metaenvironment
--
--    TODO(sushi) move this into its own file as it grows
-- ]]


M.lpp = {}
local lpp = M.lpp

-- returns the indentation preceeding the current token
lpp.indentation = function()
	local i = C.get_token_indentation(lpp.handle, macro_token_index)
	return ffi.string(i.s, i.len)
end

lpp.parse_file = function(path)
	local mpb = C.process_file(lpp.handle, make_str(path))

	if mpb.memhandle == nil then
		error("",2)
	end

	local result = buffer.new(mpb.memsize)
	local buf = result:ref()

	C.get_metaprogram_result(mpb, buf)

	return ffi.string(buf, mpb.memsize)
end

lpp.source_location = function()
	local sl = C.get_token_source_location(lpp.handle, macro_token_index)
	return ffi.string(sl.streamname.s, sl.streamname.len), sl.line, sl.column
end

-- Document 'cursor', used for iterating the document parts of the 
-- file from within a macro and possibly consuming pieces of the document.
local Cursor = {}
Cursor.__index = Cursor

Cursor.new = function(offset)
	local o = {}
	o.offset = offset
	setmetatable(o, Cursor)
	return o
end

Cursor.codepoint = function(self)
	return C.codepoint_at_offset(lpp.handle, self.offset)
end

-- Moves the cursor forward by a single character.
Cursor.next_char = function(self)
	self.offset = self.offset + C.advance_at_offset(lpp.handle, self.offset)
end

Cursor.source_location = function(self)
	local sl = C.get_source_location(self.offset)
	return ffi.string(sl.streamname.s, sl.streamname.len), sl.line, sl.column
end

lpp.get_cursor_after_macro = function()
	
end

return M, lpp
