local lpp = require "lpp"
local List = require "list"
local ctx = require "ctx"
local CGen = require "cgen"

---@param c CGen
local primitivePrint = function(c, name)
  c:append('io::formatv(io, "', name, ': ", ', name, ', \'\\n\');')
end

local printers = 
{
  u8  = primitivePrint,
  u16 = primitivePrint,
  u32 = primitivePrint,
  u64 = primitivePrint,
  s8  = primitivePrint,
  s16 = primitivePrint,
  s32 = primitivePrint,
  s64 = primitivePrint,
  str = primitivePrint,
}

return function()
  local sec = 
    assert(lpp.getSectionAfterMacro(),
      "struct called but is not followed by a section!")

  -- Insert missing 'struct'
  sec:insertString(0, "struct ")

  local text = sec:getString()

  ctx:loadString(text)

  local result = ctx:parseTopLevelDecl()

  ---@type Decl
  local decl = result.decl

  assert(decl, 
    "failed to parse struct")

  local declname = decl:name()

  -- Write io formatter
  -- Note that this is done first as the end offset of the structure
  -- will be different after we place the print member.
  local c = CGen.new()

  c:append("")

  c:beginNamespace("iro::io")
  c:beginFunction(
    "s64", "format", "io::IO* io", 
    declname.."* x")
  do
    c:append("x->print(io);")
  end
  c:endFunction()
  c:endNamespace()

  sec:insertString(decl:getEnd() + 2, c.buffer:get())

  c = CGen.new()
    
  c:indent()

  c:beginFunction("void", "print", "io::IO* io")
  do
    ---@type DeclIter
    local fields = assert(decl:getDeclIter())
    while true do
      local field = fields:next()
      if not field then break end

      local fieldname = field:name()

      local type = field:type()
      local typename = type:getTypeName()
      
      c:append(
        'io::formatv(io, "', fieldname, ': ", ', fieldname, ', \'\\n\');')
    end
  end
  c:endFunction()

  sec:insertString(decl:getEnd(), c.buffer:get())

end
