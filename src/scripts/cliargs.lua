local output = "src/generated/cliargs.h"

local CGen = require "src.scripts.cgen"

local enums = CGen.new()
local functions = CGen.new()

local double_dash =
{
	{ "verbosity",
		option = "<level>",
		description = "set the verbosity <level> of the internal logger to one of: trace, debug, info, notice, warn, error, fatal"
	},
	{ "start-lsp",
		description = "start the language server instead of invoking lpp."
	},
	{ "help",
		description = "display this help message."
	},
}

local single_dash =
{
	{"v",
		description = "set the verbosity of the internal logger to 'debug'."
	},
}

local verbosity_options =
{
	{ "trace"  },
	{ "debug"  },
	{ "info"   },
	{ "notice" },
	{ "warn"   },
	{ "error"  },
	{ "fatal"  },
}

local Trie =
{
	root = nil,

	new_node = function()
		return {child_count = 0, children = {}}
	end,

	add = function(self, s)
		local node = self.root
		for i=1,#s do
			local c = s:sub(i,i)
			if not node.children[c] then
				node.children[c] = self.new_node()
			end
			node.child_count = node.child_count + 1
			node = node.children[c]
		end
		return node
	end,
}
Trie.__index = Trie

Trie.new = function()
	local o = {}
	o.root = Trie.new_node()
	setmetatable(o, Trie)
	return o
end

local generate = function(mapping, enum_name, func_name)
	local trie = Trie.new()

	local func = CGen.new()
	func:begin_function("static "..enum_name, func_name, "const char* arg")

	local enum = CGen.new()
	enum:begin_enum(enum_name)
	enum:append_enum_element("Unknown")

	for _,k in ipairs(mapping) do
		local o = k[1]
		local e = k[2] or o

		if not k[2] then
			-- sanitize name of some nono characters in C
			e = e:gsub("%-", "_")
		end

		local node = trie:add(o)
		enum:append_enum_element(e)

		node.fin = function()
			func:append("return "..enum_name.."::"..e..";")
		end
	end

	enum:end_enum()

	local walk
	walk = function(node)
		func:begin_switch("*arg")
		if node.child_count == 0 then
			if not node.fin then
				error("encountered terminal trie node with no fin()")
			end

			func:begin_case("\\0")
				node.fin()
			func:end_case()
		else
			if node.fin then
				func:begin_default_case()
					node.fin()
				func:end_default_case()
			end
			for k,v in pairs(node.children) do
				func:begin_case(k)
					func:append("arg += 1;")
					walk(v)
				func:end_case()
			end
		end
		func:end_switch()
	end

	walk(trie.root)

	func:append("return "..enum_name.."::Unknown;")
	func:end_function()

	return func.buffer:get(), enum.buffer:get()
end


local ddf, dde = generate(double_dash, "DoubleDashArg", "parse_double_dash_arg")
local sdf, sde = generate(single_dash, "SingleDashArg", "parse_single_dash_arg")
local vof, voe = generate(verbosity_options, "VerbosityOption", "parse_verbosity_option")

local help = ""

local longest_namelen = 0
for _,v in ipairs(double_dash) do
	local name = "--"..v[1].." "

	if v.option then
		name = name..v.option.." "
	end

	if #name > longest_namelen then
		longest_namelen = #name
	end

	v.full_name = name
end

for _,v in ipairs(double_dash) do
	help = help.."  "..v.full_name

	for _=1,longest_namelen - #v.full_name do
		help = help.." "
	end

	help = help..v.description.."\n"
end

for _,v in ipairs(single_dash) do
	local full_name = "-"..v[1]

	help = help.."  "..full_name

	for _=1,longest_namelen - #full_name do
		help = help.." "
	end

	help = help..v.description.."\n"
end

help = "static const char* cliargs_list_str = R\"help_message("..help..")help_message\";\n"

local f = io.open(output, "w")

if not f then
	error("failed to open '"..output.."' for writing")
end

f:write(dde, ddf,
        sde, sdf,
		voe, vof,
		help)

f:close()

--io.write(
--	dde, ddf,
--	sde, sdf,
--	voe, vof)
