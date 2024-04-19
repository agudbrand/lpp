--
--
-- Helper for generating C code in a consistently formatted way
--
--

local buffer = require "string.buffer"

---@class CGen
local CGen =
{
	buffer = buffer.new(),

	depth = 0,

	append_indentation = function(self)
		for _=1,self.depth do
			self.buffer:put(" ")
		end
	end,

	begin_enum = function(self, name)
		self:append_indentation()
		self.buffer:put("enum ", name, "\n")
		self:begin_scope()
	end,

	append_enum_element = function(self, name)
		self:append_indentation()
		self.buffer:put(name, ",\n")
	end,

	end_enum = function(self)
		self.depth = self.depth - 1
		self:append_indentation()
		self.buffer:put("};\n")
	end,

	begin_switch = function(self, c)
		self:append_indentation()
		self.buffer:put("switch (", c, ")\n")
		self:begin_scope()
	end,

	end_switch = function(self)
		self:end_scope()
	end,

	begin_case = function(self, c)
		self:append_indentation()
		self.buffer:put("case '", c, "':\n")
		self.depth = self.depth + 1
	end,

	end_case = function(self)
		self:append_indentation()
		self.buffer:put("break;\n")
		self.depth = self.depth - 1
	end,

	begin_default_case = function(self)
		self:append_indentation()
		self.buffer:put("default:\n")
		self.depth = self.depth + 1
	end,

	end_default_case = function(self)
		self:append_indentation()
		self.buffer:put("break;\n")
		self.depth = self.depth - 1
	end,

	begin_if = function(self, cond)
		self:append_indentation()
		self.buffer:put("if (", cond, ")\n")
		self:begin_scope()
	end,

	-- expected to match an 'if'
	-- also ended with 'end_if'
	begin_else = function(self)
		self:end_if()
		self:append_indentation()
		self.buffer:put("else\n")
		self:begin_scope()
	end,

	begin_else_if = function(self, cond)
		self:end_if()
		self:append_indentation()
		self.buffer:put("else if (", cond, ")\n")
		self:begin_scope()
	end,

	end_if = function(self)
		self:end_scope()
	end,

	begin_function = function(self, return_type, name, ...)
		self:append_indentation()
		self.buffer:put(return_type, " ", name, "(")
		local args = {...}
		if #args ~= 0 then
			for _,arg in ipairs(args) do
				self.buffer:put(arg)
			end
		end
		self.buffer:put(")\n")
		self:begin_scope()
	end,

	end_function = function(self)
		self:end_scope()
	end,

	begin_infinite_loop = function(self)
		self:append_indentation()
		self.buffer:put("for (;;)\n")
		self:begin_scope()
	end,

	end_infinite_loop = function(self)
		self:end_scope()
	end,

	begin_scope = function(self)
		self:append_indentation()
		self.buffer:put("{\n")
		self.depth = self.depth + 1
	end,

	end_scope = function(self)
		self.depth = self.depth - 1
		self:append_indentation()
		self.buffer:put("}\n")
	end,

	-- note that this appends by line 
	append = function(self, ...)
		self:append_indentation()
		self.buffer:put(...)
		self.buffer:put("\n")
	end,
}
CGen.__index = CGen

--@return CGen
CGen.new = function()
	local o = {}
	setmetatable(o, CGen)
	o.buffer = buffer.new()
	o.depth = 0
	return o
end

return CGen
