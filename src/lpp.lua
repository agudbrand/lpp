--
--
-- lpp api
--
--

local ffi = require "ffi"
local C = ffi.C

local buffer = require "string.buffer"

local List = require "list"
local Twine = require "twine"

local strtype = ffi.typeof("str")
local strToLua = function(s)
	return ffi.string(s.s, s.len)
end
local luaToStr = function(s)
	return strtype(s, #s)
end

local makeStruct = function()
	local o = {}
	o.__index = o
	return o
end

local MacroExpansion,
	  MacroPart,
	  Section

-- * ==============================================================================================
-- *   lpp
-- * ==============================================================================================

--- API for interacting with lpp during the execution of a metaprogram.
---
---@class lpp
--- Handle to the internal representation of lpp.
---@field handle userdata 
--- Handle to the currently executing metaprogram's 'context'.
---@field context userdata
local lpp = {}

-- * ----------------------------------------------------------------------------------------------

--- Get the indentation of the current macro as a string.
---
--- Good for keeping things nicely formatted in macro expansions that span multiple 
--- lines. Please use it ^_^.
---
---@return string
lpp.getMacroIndentation = function()
	return strToLua(C.metaprogramGetMacroIndent(lpp.context))
end

-- * ----------------------------------------------------------------------------------------------

--- Process the file at 'path' with lpp. 
---
--- Returns the final output of the evaluated file as a string.
---
---@param path string
---@return string
lpp.processFile = function(path)
	local mpb = assert(C.processFile(lpp.handle, luaToStr(path)),
					"failed to get memory buffer from processFile")

	local result = buffer.new(mpb.memsize)
	local buf = result:ref()

	C.getMetaprogramResult(mpb, buf)

	return ffi.string(buf, mpb.memsize)
end

-- * ----------------------------------------------------------------------------------------------

--- Retrieve the section after the calling macro, or nil if none follows.
---
---@return Section?
lpp.getSectionAfterMacro = function()
	local s = C.metaprogramGetNextSection(lpp.context)
	if s ~= nil then
		return Section.new(s)
	end
end

-- * ----------------------------------------------------------------------------------------------

--- Get a string of the metaprogram's output so far.
---
---@return string
lpp.getOutputSoFar = function()
	return strToLua(C.metaprogramGetOutputSoFar(lpp.context))
end

-- * ==============================================================================================
-- *   Section
-- * ==============================================================================================

--- A part of the Document being processed. 
---
--- Currently, this represents either a Document section, or a Macro section.
---
---@class Section
--- Handle to the internal representation of a Section.
---@field handle userdata
Section = makeStruct()
lpp.Section = Section

-- * ----------------------------------------------------------------------------------------------

---@private
--
-- Construct a new Section.
--
---@param handle userdata
Section.new = function(handle)
	return setmetatable({handle=handle}, Section)
end

-- * ----------------------------------------------------------------------------------------------

--- Get the Section following this one, or nil if this is the last
--- Section of the Document.
---
---@return Section?
Section.getNextSection = function(self)
	local s = C.sectionNext(self.handle)
	if s ~= nil then
		return Section.new(s)
	end
end

-- * ----------------------------------------------------------------------------------------------

--- Get the Section preceeding this one, or nil if this is the first
--- Section of the Document.
---
---@return Section?
Section.getPrevSection = function(self)
	local s = C.sectionNext(self.handle)
	if s ~= nil then
		return Section.new(s)
	end
end

-- * ----------------------------------------------------------------------------------------------

--- Check if this Section is a Macro.
---
---@return boolean
Section.isMacro = function(self) return 0 ~= C.sectionIsMacro(self.handle) end

-- * ----------------------------------------------------------------------------------------------

--- Check if this Section is Document text.
---
---@return boolean
Section.isDocument = function(self) return 0 ~= C.sectionIsDocument(self.handle) end

-- * ----------------------------------------------------------------------------------------------

--- Gets a lua string containing the contents of this Section.
--- 
--- Returns nil if the section is not a Document.
---
---@return string?
Section.getString = function(self)
	if not self:isDocument() then return nil end
	return strToLua(C.sectionGetString(self.handle))
end

-- * ----------------------------------------------------------------------------------------------

--- Insert text at the given offset in this section.
---
---@param offset number
---@param text string
---@return boolean
Section.insertString = function(self, offset, text)
	return 0 ~= C.sectionInsertString(self.handle, offset, luaToStr(text))
end

-- * ----------------------------------------------------------------------------------------------

--- Consume some bytes from the beginning of this section.
---
---@param len number
---@return boolean
Section.consumeFromBeginning = function(self, len)
	return 0 ~= C.sectionConsumeFromBeginning(self.handle , len)
end

-- * ==============================================================================================
-- *   MacroExpansion
-- * ==============================================================================================

--- The result of a macro invocation. This contains a sequence
--- of either strings and/or MacroParts that form the buffer that 
--- will replace the macro.
---
--- Currently these only work when they are the returned value 
--- of a macro.
---
---@class MacroExpansion
--- A list of string or MacroPart that will be joined to form the 
---@field list List 
MacroExpansion = makeStruct()
lpp.MacroExpansion = MacroExpansion

-- * ----------------------------------------------------------------------------------------------

--- Check if 'x' is a MacroExpansion.
---
---@param x any
---@return boolean
MacroExpansion.isTypeOf = function(x)
	return getmetatable(x) == MacroExpansion
end

-- * ----------------------------------------------------------------------------------------------

--- Construct a new MacroExpansion.
---
--- An initial sequence of strings and/or MacroParts may be passed.
---
---@param ... string | MacroPart
---@return MacroExpansion
MacroExpansion.new = function(...)
	return setmetatable(
	{
		list = List{...}
	}, MacroExpansion)
end

-- * ----------------------------------------------------------------------------------------------

--- Push a string or MacroPart to the front of this expansion.
---
---@param v string | MacroPart
---@return self
MacroExpansion.pushFront = function(self, v)
	self.list:pushFront(v)
	return self
end

-- * ----------------------------------------------------------------------------------------------

--- Push a string or MacroPart to the back of this expansion.
---
---@param v string | MacroPart
---@return self
MacroExpansion.pushBack = function(self, v)
	self.list:push(v)
	return self
end

-- * ----------------------------------------------------------------------------------------------

---@private
-- Call metamethod used internally to easily finalize a macro expansion.
-- When a macro returns a MacroExpansion it is immediately called.
--
---@param offset number The offset into the buffer this macro was used in.
MacroExpansion.__call = function(self, offset)
	local out = buffer.new()

	self.list:each(function(x)
		if MacroPart.isTypeOf(x)
		then
			C.metaprogramTrackExpansion(lpp.context, x.start, offset + #out)
			out:put(tostring(x))
		else
			out:put(x)
		end
	end)

	return out:get()
end

-- * ----------------------------------------------------------------------------------------------

--- Override the concat operator to make MacroExpansion sorta able to 
--- behave like a normal string, in the same way MacroPart does. 
---
--- MacroExpansion can concat with a string, MacroPart, or another 
--- MacroExpansion. 
---
-- Note that MacroPart will never appear as 'lhs' here, because it 
-- defines its own __concat operator.
--
-- Also, lots of silly @casts here. Gonna keep it for now as it silences
-- luals's huge amount of warnings here, and I think its neat. Would be 
-- a lot nicer if it were capable of inferring type correctly from my
-- isTypeOf functions, though.
---
---@param lhs MacroExpansion | string
---@param rhs MacroExpansion | MacroPart | string
---@return MacroExpansion
MacroExpansion.__concat = function(lhs, rhs)
	local tryToString = function(from, to)
		assert(getmetatable(from), "attempt to concatenate a plain table with a MacroExpansion!")
		assert(getmetatable(from).__tostring,
			"attempt to concatenate a table with no '__tostring' method with a MacroExpansion!")

		return to:pushBack(tostring(from))
	end

	if MacroExpansion.isTypeOf(lhs) 
	then ---@cast lhs -string
		if MacroPart.isTypeOf(rhs) or
		   "string" == type(rhs)
		then ---@cast rhs -MacroExpansion
		 	return lhs:pushBack(rhs)
		elseif MacroExpansion.isTypeOf(rhs)
		then ---@cast rhs MacroExpansion
			rhs.list:each(function(x)
				lhs:pushBack(x)
			end)
			return lhs
		else
			return tryToString(rhs, lhs)
		end
	else ---@cast lhs -MacroExpansion
		 ---@cast rhs MacroExpansion
		if "string" == type(lhs)
		then
			return rhs:pushFront(lhs)
		else
			return tryToString(lhs, rhs)
		end
	end
end

-- * ==============================================================================================
-- *   MacroPart
-- * ==============================================================================================

--- A part of a MacroExpansion that contains the text to expand and information 
--- that can be used to determine where that text comes from.
---
---@class MacroPart
--- A string naming where this part comes from. This may be any name, but ideally
--- it is a path relative to the file the macro is being invoked in.
---@field source string 
--- The byte offset into 'source' where this part begins.
---@field start number
--- The byte offset into 'source' where this part ends.
---@field stop number
--- The text this part represents.
---@field text string
MacroPart = makeStruct()
lpp.MacroPart = MacroPart

-- * ----------------------------------------------------------------------------------------------

--- Check if 'x' is a MacroPart
---
---@param x any
---@return boolean
MacroPart.isTypeOf = function(x)
	return getmetatable(x) == MacroPart
end

-- * ----------------------------------------------------------------------------------------------

--- Construct a new MacroPart
---
---@param source string
---@param start number
---@param stop number
---@param text string
---@return MacroPart
MacroPart.new = function(source, start, stop, text)
	return setmetatable(
	{
		source=source,
		start=start,
		stop=stop,
		text=text,
	}, MacroPart)
end

-- * ----------------------------------------------------------------------------------------------

--- Let MacroParts coerce to strings
MacroPart.__tostring = function(self)
 	return self.text
end

-- * ----------------------------------------------------------------------------------------------

--- Consume a MacroPart and a string/MacroPart into a MacroExpansion or 
--- give this MacroPart to a MacroExpansion.
---
---@param lhs MacroPart | string
---@param rhs MacroPart | MacroExpansion | string
---@return MacroExpansion
MacroPart.__concat = function(lhs, rhs)
	if MacroExpansion.isTypeOf(rhs)
	then return MacroExpansion.__concat(lhs, rhs)
	else return MacroExpansion.__concat(lhs, MacroExpansion.new(rhs))
	end
end

return lpp
