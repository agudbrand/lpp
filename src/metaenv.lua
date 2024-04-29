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
	C.metaenvironment_add_document_section(lpp.context, start, make_str(str))
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
	C.metaenvironment_add_document_section(lpp.context, start, make_str(s))
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
	C.metaenvironment_add_macro_section(lpp.context, start)
end

menv.final = function()
	menv.output = C.source_create(lpp.handle, make_str("output"))
	menv.cursec.stop = menv.cursec.start + #menv.cursec.buf
	menv.cursec = menv.sec_list
	while menv.cursec do
		local sec = menv.cursec
		if sec.type == "document" then
			table.insert(menv.source_map, {C.source_get_cache_len(menv.output), sec.start})
			local ptr, len = sec.buf:ref()
			C.source_write_cache(menv.output, Bytes(ptr, len))
		elseif sec.type =="macro" then
			local res = sec.macro(unpack(sec.args))
			if res then
				table.insert(menv.source_map, {C.source_get_cache_len(menv.output), sec.start})
				C.source_write_cache(menv.output, Bytes(res, #res))
			end
		end
		menv.cursec = menv.cursec.next
	end

	local source = C.metaprogram_get_source(lpp.handle)
	C.source_cache_line_offsets(menv.output)

	for _,v in ipairs(menv.source_map) do
		local ogloc = C.source_get_loc(source, v[2])
		local nuloc = C.source_get_loc(menv.output, v[1])

		local f = function(x)
			return tostring(tonumber(x))
		end

		io.write(f(ogloc.line), ":", f(ogloc.column), "(", v[2], ") -> ", f(nuloc.line), ":", f(nuloc.column), "(", f(v[1]), ")\n")
	end

	local len = C.source_get_cache_len(menv.output)
	local s = C.source_get_str(menv.output, 0, len)

	return ffi.string(s.s, s.len)
end


-- [[
--    User facing lpp api, accessed via the lpp var anywhere in the metaenvironment
--
--    TODO(sushi) move this into its own file as it grows
-- ]]


-- returns the indentation preceeding the current token
lpp.indentation = function()
	local i = C.get_token_indentation(lpp.handle, macro_token_index)
	return ffi.string(i.s, i.len)
end

lpp.process_file = function(path)
	local mpb = C.process_file(lpp.context, make_str(path))

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

Cursor.new = function()
	local o = {}
	o.handle = C.metaenvironment_new_cursor_after_section(lpp.context)
	setmetatable(o, Cursor)
	ffi.gc(o.handle, function(handle)
		C.metaenvironment_delete_cursor(lpp.context, handle)
	end)
	return o
end

Cursor.codepoint = function(self)
	return C.metaenvironment_cursor_current_codepoint(lpp.context, self.handle)
end

-- Moves the cursor forward by a single character.
-- This will skip any macros encountered until it reaches 
-- either the next document section or the end of the current source.
Cursor.next_char = function(self)
	return 0 ~= C.metaenvironment_cursor_next_char(lpp.context, self.handle)
end

lpp.get_output_so_far = function()
	return menv.output:tostring()
end

lpp.get_cursor_after_macro = function()
	return Cursor.new()
end

return M, lpp
