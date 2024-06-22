--
--
-- lpp extension providing access to clang.
--
--

-- load the generated cdefs file and give them to ffi

local ffi = require "ffi"

ffi.cdef [[
typedef enum
{
 DeclKind_Invalid,
 DeclKind_Unknown,
 DeclKind_Function,
 DeclKind_Method,
 DeclKind_Parameter,
 DeclKind_Variable,
 DeclKind_Field,
 DeclKind_Typedef,
 DeclKind_Record,
 DeclKind_Enum,
} DeclKind;
typedef enum
{
 TK_EndOfFile,
 TK_Unhandled,
 TK_Comment,
 TK_Whitespace,
 TK_Identifier,
 TK_NumericConstant,
 TK_CharConstant,
 TK_StringLiteral,
} TokenKind;
typedef struct Lexer Lexer;
typedef struct Token Token;
typedef struct Context Context;
typedef struct Decl Decl;
typedef struct Type Type;
typedef struct DeclIter DeclIter;
typedef struct ParamIter ParamIter;
typedef struct FieldIter FieldIter;
typedef struct EnumIter EnumIter;
typedef struct
{
 str raw;
 u64 start_offset;
 u64 end_offset;
} TokenRawAndLoc;
void addIncludeDir(Context* ctx, str s);
Type* lookupType(Context* ctx, str name);
Context* createContext();
void destroyContext(Context*);
Lexer* createLexer(Context* ctx, str s);
Token* lexerNextToken(Lexer* l);
str tokenGetRaw(Context* ctx, Lexer* l, Token* t);
TokenKind tokenGetKind(Token* t);
b8 createASTFromString(Context* ctx, str s);
Decl* parseString(Context* ctx, str s);
void dumpDecl(Decl* decl);
Decl* getTranslationUnitDecl(Context* ctx);
DeclIter* createDeclIter(Context* ctx, Decl* decl);
Decl* getNextDecl(DeclIter* iter);
DeclKind getDeclKind(Decl* decl);
str getDeclName(Decl* decl);
Type* getDeclType(Decl* decl);
Type* getTypeDeclType(Context* ctx, Decl* decl);
u64 getDeclBegin(Context* ctx, Decl* decl);
u64 getDeclEnd(Context* ctx, Decl* decl);
Type* getFunctionReturnType(Decl* decl);
b8 functionHasBody(Decl* decl);
u32 getFunctionBodyBegin(Decl* decl);
u32 getFunctionBodyEnd(Decl* decl);
b8 tagHasBody(Decl* decl);
u32 getTagBodyBegin(Decl* decl);
u32 getTagBodyEnd(Decl* decl);
b8 tagIsEmbeddedInDeclarator(Decl* decl);
ParamIter* createParamIter(Context* ctx, Decl* decl);
Decl* getNextParam(ParamIter* iter);
b8 isCanonical(Type* type);
b8 isUnqualified(Type* type);
b8 isUnqualifiedAndCanonical(Type* type);
b8 isConst(Type* type);
Type* getCanonicalType(Type* type);
Type* getUnqualifiedType(Type* type);
Type* getUnqualifiedCanonicalType(Type* type);
str getTypeName(Context* ctx, Type* type);
str getCanonicalTypeName(Context* ctx, Type* type);
str getUnqualifiedCanonicalTypeName(Context* ctx, Type* type);
u64 getTypeSize(Context* ctx, Type* type);
b8 typeIsBuiltin(Type* type);
Decl* getTypeDecl(Type* type);
FieldIter* createFieldIter(Context* ctx, Decl* decl);
Decl* getNextField(FieldIter* iter);
u64 getFieldOffset(Context* ctx, Decl* field);
EnumIter* createEnumIter(Context* ctx, Decl* decl);
Decl* getNextEnum(EnumIter* iter);
void dumpAST(Context* ctx);
str getClangDeclSpelling(Decl* decl);
]]

-- set to the lpp object when loaded
local lpp = require "lpp"

-- contains the shared library when loaded 
local lppclang

-- helpers
local strtype = ffi.typeof("str")
local make_str = function(s)
  return strtype(s, #s)
end

local strToLua = function(s)
  return ffi.string(s.s, s.len)
end

--- General api for using lppclang.
local clang = {}

-- forward declarations
local Ctx,
      Decl,
    Type,
    Function,
    DeclIter,
    ParamIter,
    FieldIter,
    EnumIter,
    Lexer,
    Token

local makeStruct = function()
  local o = {}
  o.__index = o
  return o
end

--- A clang context which, at the moment, allows incrementally 
--- parsing C++ code.
---
---@class Ctx
---@field handle userdata Handle to the internal representation of this Ctx
Ctx = makeStruct()

--- Create a new lppclang context.
---
---@return Ctx
Ctx.new = function()
  local o = setmetatable({}, Ctx)

  o.handle = assert(lppclang.createContext(), "failed to create lppclang context!")

  -- automatically clean up the context
  ffi.gc(o.handle, function(self)
    lppclang.destroyContext(self.handle)
  end)

  return o
end

--- Parse a given string using the given context.
--- This currently assumes that what is being parsed is in global
--- scope, but supports some extra stuff I'll have to document later.
---
--- A Decl representing the fake translation unit will be returned.
---
-- TODO(sushi) maybe just return the DeclIter over the TU instead
--
---@param code string
---@return Decl
Ctx.parseString = function(self, code)
  return Decl.new(self,
    assert(lppclang.parseString(self.handle, make_str(code)),
      "failed to parse given string!"))
end

--- Lex a given string using this context. Note that this does NOT parse
--- the string. This uses a 'raw' lexer, which means that it literally 
--- just gives tokens, no preprocessing or anything like that is done.
---
---@param code string
---@return Lexer
Ctx.lexString = function(self, code)
  return Lexer.new(self,
    assert(lppclang.createLexer(self.handle, make_str(code)),
      "failed to create a Lexer for code \n"..code))
end

--- Lex a given section. This will automatically consume the section 
--- as tokens are retrieved. 
---
-- TODO(sushi) add an options table to thit
---
---@param section Section
---@return Lexer
Ctx.lexSection = function(self, section)
  local lex = self:lexString(assert(section:getString()))
  lex.section = section
  lex.consume = true
  return lex
end

--- Add a path to the include search list.
--- The path must exist, otherwise the program will crash (for now).
---
---@param path string
Ctx.addIncludeDir = function(self, path)
  lppclang.addIncludeDir(self.handle, make_str(path))
end

--- A parsed declaration.
---
---@class Decl
---@field ctx Ctx The context this Decl was parsed in.
---@field handle userdata Handle to the internal representation of this Decl.
Decl = makeStruct()

--- Construct a new Decl in ctx.
---
---@param ctx Ctx
---@param handle userdata
Decl.new = function(ctx, handle)
  return setmetatable(
  {
    ctx = ctx,
    handle = handle
  }, Decl)
end

--- Assuming that this declaration acts as a context for other declarations,
--- creates and returns a DeclIter over its child decls.
---
--- Returns nil on failure.
---
---@return DeclIter?
Decl.getDeclIter = function(self)
  local iter = lppclang.createDeclIter(self.ctx.handle, self.handle)
  if iter == nil then
    return
  end

  return DeclIter.new(self.ctx, iter)
end

--- Get a string of the name of this Decl
---
---@return string
Decl.name = function(self)
  local n = lppclang.getDeclName(self.handle)
  return ffi.string(n.s, n.len)
end

--- Retrieve the Type of this declaration.
---
---@return Type
Decl.type = function(self)
  local t = lppclang.getDeclType(self.handle)
  return Type.new(self.ctx, t)
end

--- Get the offset into the string this Decl was parsed from that
--- it begins at. 
---
---@return number
Decl.getBegin = function(self)
  return assert(tonumber(lppclang.getDeclBegin(self.ctx.handle, self.handle)))
end

--- Get the offset into the string this Decl was parsed from that
--- it ends at.
---
---@return number
Decl.getEnd = function(self)
  return assert(tonumber(lppclang.getDeclEnd(self.ctx.handle, self.handle)))
end

--- Test if this Decl is an enum.
---
---@return boolean
Decl.isEnum = function(self)
  return lppclang.getDeclKind(self.handle) == lppclang.DeclKind_Enum
end

--- Test if this Decl is a struct.
---
---@return boolean
Decl.isStruct = function(self)
  return lppclang.getDeclKind(self.handle) == lppclang.DeclKind_Record
end

--- Test if this Decl is a variable.
---
---@return boolean
Decl.isVariable = function(self)
  return lppclang.getDeclKind(self.handle) == lppclang.DeclKind_Variable
end

--- Test if this Decl is a field, a variable that is a part of a struct.
---
---@return boolean
Decl.isField = function(self)
  return lppclang.getDeclKind(self.handle) == lppclang.DeclKind_Field
end

--- Given that this Decl is an enum, creates and returns an iterator
--- over the enum's elements.
---
---@return EnumIter
Decl.getEnumIter = function(self)
  assert(self:isEnum(), "Decl.getEnumIter called on a decl that is not an enum")

  return EnumIter.new(self.ctx, assert(lppclang.createEnumIter(self.ctx.handle, self.handle), "failed to create an enum iter!"))
end

--- If this is a Tag declaration, eg. one that is named (struct, enum, etc.)
--- check if it is embdedded in declarators.
---
--- Eg. this returns true for
--- ```cpp
--- struct Apple {} apple;
--- ```
--- and false for 
--- ```cpp
--- struct Apple {};
--- ```
---
---@return boolean
Decl.tagIsEmbeddedInDeclarator = function(self)
  return 0 ~= lppclang.tagIsEmbeddedInDeclarator(self.handle)
end

Decl.asFunction = function(self)
  if lppclang.getDeclKind(self.decl) == lppclang.DeclKind_Function then
    return Function.new(self.ast, self.decl)
  end
end


--- An iterator over a sequence of declarations.
---
---@class DeclIter
---@field ctx Ctx
---@field handle userdata Handle to the internal representation of this DeclIter
DeclIter = makeStruct()

--- Construct a new DeclIter.
---
---@param ctx Ctx 
---@param handle userdata
DeclIter.new = function(ctx, handle)
  return setmetatable(
  {
    ctx = ctx,
    handle = handle,
  }, DeclIter)
end

--- Retrieve the next Decl in the sequence, or nil if there's
--- no more.
---
---@return Decl?
DeclIter.next = function(self)
  local nd = lppclang.getNextDecl(self.handle)
  if nd ~= nil then
    return Decl.new(self.ctx, nd)
  end
end

--- A C/C++ type.
---
---@class Type
---@field ctx Ctx
---@field handle userdata
Type = makeStruct()

--- Construct a new Type
---
---@param ctx Ctx
---@param handle userdata
Type.new = function(ctx, handle)
  return setmetatable(
  {
    ctx = ctx,
    handle = handle
  }, Type)
end

--- Check if this type is canonical.
---
---@return boolean
Type.isCanonical = function(self)
  return 0 ~= lppclang.isCanonical(self.handle)
end

--- Check if this type is unqualified.
---
---@return boolean
Type.isUnqualified = function(self)
  return 0 ~= lppclang.isUnqualified(self.handle)
end

--- Check if this type is unqualified and canonical.
---
---@return boolean
Type.isUnqualifiedAndCanonical = function(self)
  return 0 ~= lppclang.isUnqualifiedAndCanonical(self.handle)
end

--- Check if this type is const.
---
---@return boolean
Type.isConst = function(self)
  return 0 ~= lppclang.isConst(self.handle)
end

--- Retrieve the canonical representation of this Type.
---
---@return Type
Type.getCanonicalType = function(self)
  return Type.new(self.ctx, lppclang.getCanonicalType(self.handle))
end

--- Retrieve the unqualified representation of this Type.
---
---@return Type
Type.getUnqualifiedType = function(self)
  return Type.new(self.ctx, lppclang.getUnqualifiedType(self.handle))
end

--- Retrieve the unqualified canonical representation of this Type.
---
---@return Type
Type.getUnqualifiedCanonicalType = function(self)
  return Type.new(self.ctx, lppclang.getUnqualifiedCanonicalType(self.handle))
end

--- Retrieve the size in bytes of this type.
---
---@return number
Type.size = function(self)
  return assert(tonumber(lppclang.getTypeSize(self.ctx.handle, self.handle)))
end

--- Check if this type is a builting C/C++ type.
---
---@return boolean
Type.isBuiltin = function(self)
  return 0 ~= lppclang.typeIsBuiltin(self.handle)
end

Type.getDecl = function(self)
  return lppclang.getTypeDecl(self.handle)
end

Type.getTypeName = function(self)
  return strToLua(lppclang.getTypeName(self.ctx.handle, self.handle))
end

Type.getCanonicalTypeName = function(self)
  return strToLua(lppclang.getCanonicalTypeName(self.ctx.handle, self.handle))
end

Type.getUnqualifiedCanonicalTypeName = function(self)
  return strToLua(lppclang.getUnqualifiedCanonicalTypeName(self.ctx.handle, self.handle))
end

EnumIter = {}
EnumIter.__index = EnumIter

EnumIter.new = function(ast, iter)
  assert(iter, "EnumIter.new called without an enum iter pointer")

  local o = {}
  setmetatable(o, EnumIter)
  o.ast = ast
  o.iter = iter
  return o
end

EnumIter.next = function(self)
  local ne = lppclang.getNextEnum(self.iter)
  if ne == nil then
    return nil
  end
  return Decl.new(self.ast, ne)
end

--- An instance of a clang Lexer over some string.
---
---@class Lexer
---@field ctx Ctx
---@field handle userdata 
---@field curt userdata
---@field section Section
---@field consume boolean
---@field keep_whitespace boolean
Lexer = {}
Lexer.__index = Lexer

Lexer.new = function(ctx, handle)
  assert(ctx and handle)

  return setmetatable(
  {
    ctx=ctx,
    handle=handle,
    curt=nil,
  }, Lexer)
end

Lexer.next = function(self)
  self.curt = lppclang.lexerNextToken(self.handle)
  if self.curt ~= nil then
    if self.section and self.consume then
      self.section:consumeFromBeginning(#self:curtRaw())
    end
    if not self.keep_whitespace and self:at(Lexer.whitespace) then
      return self:next()
    end
    return true
  else
    self.curt = nil
  end
end

Lexer.curtRaw = function(self)
  return strToLua(lppclang.tokenGetRaw(self.ctx.handle, self.handle, self.curt))
end

Lexer.eof = 0
Lexer.identifier = 1
Lexer.comment = 2
Lexer.number = 3
Lexer.char = 4
Lexer.string = 5
Lexer.whitespace = 6
Lexer.other = 7

Lexer.curtKind = function(self)
  local k = lppclang.tokenGetKind(self.curt)
  if k == lppclang.TK_EndOfFile then return Lexer.eof end
  if k == lppclang.TK_Identifier then return Lexer.identifier end
  if k == lppclang.TK_Comment then return Lexer.comment end
  if k == lppclang.TK_NumericConstant then return Lexer.number end
  if k == lppclang.TK_CharConstant then return Lexer.char end
  if k == lppclang.TK_StringLiteral then return Lexer.string end
  if k == lppclang.TK_Whitespace then return Lexer.whitespace end
  return Lexer.other
end

Lexer.at = function(self, x)
  if not self.curt then return false end
  local xtype = type(x)
  if xtype == "number" then
    -- assume its a thing defined above
    return self:curtKind() == x
  elseif xtype == "string" then
    return self:curtRaw() == x
  else
    error("unhandled type passed to Lexer.curtAt: "..xtype, 2)
  end
end

-- TODO(sushi) try not to make multiple contexts or leave it to the user to manage them
local createContext = function()
  local ctx = lppclang.createContext()

  if not ctx then
    error("failed to create an lppclang context", 2)
  end

  ffi.gc(ctx, function(self)
    lppclang.destroyContext(self)
  end)

  return ctx
end

clang.lexString = function(str)
  local ctx = createContext()
  if not ctx then return end
  return Lexer.new(ctx, lppclang.createLexer(ctx, make_str(str)))
end

clang.lexSection = function(sec, options)
  options = options or {}

  local ctx = createContext()

  local l = Lexer.new(ctx, lppclang.createLexer(ctx, make_str(sec:getString())))

  l.section = sec
  l.consume = options.consume

  return l
end

clang.parseString = function(str, options)
  options = options or {}

  local ctx = createContext()

  if options.include_dirs then
    for _,d in ipairs(options.include_dirs) do
      lppclang.addIncludeDir(ctx, make_str(d))
    end
  end

  if lppclang.createASTFromString(ctx, make_str(str)) == 0 then
    error("failed to create AST from string", 2)
  end

  return AST.new(ctx)
end

clang.createContext = function()
  return Ctx.new()
end

local loaded = false

return
{

  init = function(libpath)
    if loaded then return true end

    lppclang = ffi.load(libpath)

    -- patch lpp with the clang module
    lpp.clang = clang

    loaded = true

    return true
  end

}
