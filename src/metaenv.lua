--
--
-- Metaenvironment construction function.
--
-- This is used to create an environment table that the metaprogram 
-- runs in.
--
--

local ffi = require "ffi"
local C = ffi.C

local strtype = ffi.typeof("str")
local makeStr = function(s)
	return strtype(s, #s)
end

return function(ctx)
	-- The metaenvironment table.
	-- All things in this table are accessible as 
	-- global things from within the metaprogram.
	local M = {}
	M.__index = _G

	setmetatable(M, M)

	M.__metaenv = {}
	local menv = M.__metaenv

	menv.lpp = require "lpp"

	menv.macro_table = {}

	menv.doc = function(start, str)
		C.metaprogramAddDocumentSection(ctx, start, makeStr(str))
	end

	menv.val = function(start, x)
		if x == nil then
			error("inline lua expression resulted in nil", 2)
		end
		local s = tostring(x)
		if not s then
			error("result of inline lua expression is not convertible to a string", 2)
		end
		print("adding doc ", x)
		C.metaprogramAddDocumentSection(ctx, start, makeStr(s))
	end

	menv.macro = function(start, indent, m, ...)
		local args = {...}
		if not m then
			error("macro identifier is nil", 2)
		end

		-- yeah this closure stuff seems pretty silly but 
		-- it makes getting the macro to run a lot easier.
		-- I'll need to profile this stuff later to see
		-- if it has horrible effects
		table.insert(menv.macro_table, function() return m(unpack(args)) end)
		C.metaprogramAddMacroSection(ctx, makeStr(indent), start, #menv.macro_table)
	end

	return M
end
