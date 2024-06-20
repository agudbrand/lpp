local lpp = require "lpp"
require("lppclang").init(lpp, "../lppclang/build/debug/liblppclang.so")

local Array = function()
	local ctx = lpp.clang.createContext()
	
	local lex = lpp.getSectionAfterMacro():getString()
	
end
