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

local stackcapture = require "stackcapture"

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

	menv.macro = function(start, indent, macro, ...)
		local args = {...}
		if not macro then
			error("macro identifier is nil", 2)
		end

		-- capture where this macro was invoked in phase 1
		local capture = stackcapture(1)

		local printCapture = function(cap, offset)
			offset = offset or 0
			cap:eachWithIndex(function(call, i)
				local proper_line
				local proper_src
				if call.src:sub(1,1) ~= "@" then
					proper_line = C.metaprogramMapMetaprogramLineToInputLine(ctx, call.line)
					proper_src = call.src
				else
					proper_src = call.src:sub(2)
					proper_line = call.line
				end
				io.write(proper_src, ":", proper_line, ": ")
				if call.name then
					io.write("in ", call.name, ": ")
				end
				if i ~= cap:len() - offset then
					io.write("\n")
				end
			end)
		end

		local invoker = function()
			local wrapper = function() -- uuuugh
				return macro(unpack(args))
			end
			local result = {xpcall(wrapper, function(err)
				local macro_capture = stackcapture(3)
				printCapture(capture)
				io.write("in macro specified here\n")
				printCapture(macro_capture)
				io.write("error occured in invocation: ", err, "\n")
				error({handled=true})
			end)}

			if not result[1] then
				error({handled=true})
			end

			result = result[2]

			if menv.lpp.MacroExpansion.isTypeOf(result) then
				return result()
			elseif "string" == type(result) then
				return result
			else
				printCapture(capture)
				io.write("macro returned non-string value\n")
				error({handled=true}, 0)
			end
		end

		-- yeah this closure stuff seems pretty silly but 
		-- it makes getting the macro to run a lot easier.
		-- I'll need to profile this stuff later to see
		-- if it has horrible effects
		table.insert(menv.macro_table, invoker)
		C.metaprogramAddMacroSection(ctx, makeStr(indent), start, #menv.macro_table)
	end

	return M
end
