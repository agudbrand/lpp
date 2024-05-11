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

local Bytes = ffi.typeof("Bytes")

local buffer = require "string.buffer"
local co = require "coroutine"

-- The metaenvironment table.
-- All things in this table are accessible as 
-- global things from within the metaprogram.
local M = {}
M.__index = _G

M.lpp = {}
local lpp = M.lpp

setmetatable(M, M)

M.__metaenv =
{
	-- linked list of sections of this metaprogram's source
	sec_list = {type="root"},
	-- mapping of offsets in the output file to where
	-- they originated from in the input file
	source_map = {}
}
local menv = M.__metaenv

menv.cursec = menv.sec_list

menv.data = {}


-- [[
--    Metaenvironment functions.
--    These are directly accessible by the metaprogram
--    and should not be overwritten by it.
-- ]]


local append_queue = function(elem)
	menv.cursec.next = elem
	elem.prev = menv.cursec
	menv.cursec = elem
end

local append_document = function(start, str)
	local buf = buffer.new()
	buf:put(str)
	append_queue { type = "document", start = start, buf = buf }

	--local current = menv.cursec
	--if current.type ~= "document" then
	--	current = {type = "document", start = start, buf = buffer.new()}
	--	append_queue(current)
	--end
	--current.buf:put(str)
end

menv.doc = function(start, str)
	table.insert(menv.data, "")
	C.metaenvironmentAddDocumentSection(lpp.context, start, make_str(str))
end

menv.val = function(start, x)
	if x == nil then
		error("inline lua expression resulted in nil", 2)
	end
	local s = tostring(x)
	if not s then
		error("result of inline lua expression is not convertible to a string", 2)
	end
	table.insert(menv.data, "")
	C.metaenvironmentAddDocumentSection(lpp.context, start, make_str(s))
end

menv.macro = function(start, m, ...)
	local args = {...}
	if not m then
		error("macro identifier is nil", 2)
	end
	-- yeah this closure stuff seems pretty silly but 
	-- it makes getting the macro to run a lot easier.
	-- I'll need to profile this stuff later to see
	-- if it has horrible effects
	table.insert(menv.data, function() return m(unpack(args)) end)
	C.metaenvironmentAddMacroSection(lpp.context, start)
end

-- [[
--    User facing lpp api, accessed via the lpp var anywhere in the metaenvironment
--
--    TODO(sushi) move this into its own file as it grows
-- ]]


-- returns the indentation preceeding the current token
lpp.indentation = function()
	local i = C.getTokenIndentation(lpp.handle, macro_token_index)
	return ffi.string(i.s, i.len)
end

lpp.process_file = function(path)
	local mpb = C.processFile(lpp.context, make_str(path))

	if mpb.memhandle == nil then
		error("",2)
	end

	local result = buffer.new(mpb.memsize)
	local buf = result:ref()

	C.getMetaprogramResult(mpb, buf)

	return ffi.string(buf, mpb.memsize)
end

lpp.source_location = function()
	local sl = C.getTokenSourceLocation(lpp.handle, macro_token_index)
	return ffi.string(sl.streamname.s, sl.streamname.len), sl.line, sl.column
end

-- Document 'cursor', used for iterating the document parts of the 
-- file from within a macro and possibly consuming pieces of the document.
local Cursor = {}
Cursor.__index = Cursor

Cursor.new = function()
	local o = {}
	o.handle = C.metaenvironmentNewCursorAfterSection(lpp.context)
	setmetatable(o, Cursor)
	ffi.gc(o.handle, function(handle)
		C.metaenvironmentDeleteCursor(lpp.context, handle)
	end)
	return o
end

Cursor.codepoint = function(self)
	return C.metaenvironmentCursorCurrentCodepoint(lpp.context, self.handle)
end

-- Moves the cursor forward by a single character.
-- This will skip any macros encountered until it reaches 
-- either the next document section or the end of the current source.
Cursor.next_char = function(self)
	return 0 ~= C.metaenvironmentCursorNextChar(lpp.context, self.handle)
end

Cursor.write = function(self, str)
	return 0 ~= C.metaenvironmentCursorInsertString(lpp.context, self.handle, make_str(str))
end

Cursor.get_following_string = function(self)
	local s = C.metaenvironmentCursorGetRestOfSection(lpp.context, self.handle)
	return ffi.string(s.s, s.len)
end

lpp.get_output_so_far = function()
	return menv.output:tostring()
end

lpp.get_cursor_after_macro = function()
	return Cursor.new()
end

-- API for extending various things in lpp
lpp.extapi = {}

-- Load an extension from the given path

-- Retrieve the Cursor type. This allows monkeypatching 
-- the type with extended functionality.
lpp.extapi.get_cursor_type = function()
	return Cursor
end

return M, lpp
