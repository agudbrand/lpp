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
Decl* testIncParse(str s);
void addIncludeDir(Context* ctx, str s);
Context* createContext();
void destroyContext(Context*);
Lexer* createLexer(Context* ctx, str s);
Token* lexerNextToken(Lexer* l);
str tokenGetRaw(Context* ctx, Lexer* l, Token* t);
TokenKind tokenGetKind(Token* t);
b8 createASTFromString(Context* ctx, str s);
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
local lpp 

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

-- api used by lpp user
local clang = {}

local AST,
      Decl,
	  Type,
	  Function,
	  DeclIter,
	  ParamIter,
	  FieldIter,
	  EnumIter,
	  Lexer,
	  Token

-- Wrapper around a Context* that provides access to its functions via
-- lua methods as well as some helper methods.
AST = {}
AST.__index = AST

AST.new = function(ctx)
	assert(ctx, "AST.new called without a context")

	local o = {}
	setmetatable(o, AST)
	o.ctx = ctx
	return o
end

AST.getTranslationUnitDecl = function(self) 
	local tu = lppclang.getTranslationUnitDecl(self.ctx)
	return Decl.new(self, tu)
end

AST.dump = function(self)
	lppclang.dumpAST(self.ctx)
end

Decl = {}
Decl.__index = Decl

Decl.new = function(ast, d)
	assert(d, "Decl.new called without a decl pointer")

	local o = {}
	setmetatable(o, Decl)
	o.ast = ast
	o.decl = d
	return o
end

Decl.getDeclIter = function(self)
	local iter = lppclang.createDeclIter(self.ast.ctx, self.decl)
	if iter == nil then
		return false
	end

	return DeclIter.new(self.ast, iter)
end

Decl.name = function(self)
	local n = lppclang.getDeclName(self.decl)
	return ffi.string(n.s, n.len)
end

Decl.type = function(self)
	local t = lppclang.getDeclType(self.decl)
	return Type.new(self.ast, t)
end

Decl.getBegin = function(self)
	return tonumber(lppclang.getDeclBegin(self.ast.ctx, self.decl))
end

Decl.getEnd = function(self)
	return tonumber(lppclang.getDeclEnd(self.ast.ctx, self.decl))
end

Decl.isEnum = function(self)
	return lppclang.getDeclKind(self.decl) == lppclang.DeclKind_Enum
end

Decl.isStruct = function(self)
	return lppclang.getDeclKind(self.decl) == lppclang.DeclKind_Record
end

Decl.isVariable = function(self)
	return lppclang.getDeclKind(self.decl) == lppclang.DeclKind_Variable
end

Decl.isField = function(self)
	return lppclang.getDeclKind(self.decl) == lppclang.DeclKind_Field
end

Decl.getEnumIter = function(self)
	assert(self:isEnum(), "Decl.getEnumIter called on a decl that is not an enum")

	return EnumIter.new(self.ast, lppclang.createEnumIter(self.ast.ctx, self.decl))
end

Decl.tagIsEmbeddedInDeclarator = function(self)
	return 0 ~= lppclang.tagIsEmbeddedInDeclarator(self.decl)
end

Decl.asFunction = function(self)
	if lppclang.getDeclKind(self.decl) == lppclang.DeclKind_Function then
		return Function.new(self.ast, self.decl)
	end
end

DeclIter = {}
DeclIter.__index = DeclIter

DeclIter.new = function(ast, iter)
	assert(iter, "DeclIter.new called without a decl iter pointer")

	local o = {}
	setmetatable(o, DeclIter)
	o.ast = ast
	o.iter = iter
	return o
end

DeclIter.next = function(self)
	local nd = lppclang.getNextDecl(self.iter)
	if nd == nil then
		return nil
	end
	return Decl.new(self.ast, nd)
end

Type = {}
Type.__index = Type

Type.new = function(ast, t)
	assert(ast and t)

	local o = setmetatable({}, Type)
	o.ast = ast
	o.type = t
	return o
end

Type.isCanonical = function(self)
	return 0 ~= lppclang.isCanonical(self.type)
end

Type.isUnqualified = function(self)
	return 0 ~= lppclang.isUnqualified(self.type)
end

Type.isUnqualifiedAndCanonical = function(self)
	return 0 ~= lppclang.isUnqualifiedAndCanonical(self.type)
end

Type.isConst = function(self)
	return 0 ~= lppclang.isConst(self.type)
end

Type.getCanonicalType = function(self)
	return Type.new(self.ast, lppclang.getCanonicalType(self.type))
end

Type.getUnqualifiedType = function(self)
	return Type.new(self.ast, lppclang.getUnqualifiedType(self.type))
end

Type.getUnqualifiedCanonicalType = function(self)
	return Type.new(self.ast, lppclang.getUnqualifiedCanonicalType(self.type))
end

Type.size = function(self)
	return tonumber(lppclang.getTypeSize(self.ast.ctx, self.type))
end

Type.isBuiltin = function(self)
	return 0 ~= lppclang.typeIsBuiltin(self.type)
end

Type.getDecl = function(self)
	return lppclang.getTypeDecl(self.type)
end

Type.getTypeName = function(self)
	return strToLua(lppclang.getTypeName(self.ast.ctx, self.type))
end

Type.getCanonicalTypeName = function(self)
	return strToLua(lppclang.getCanonicalTypeName(self.ast.ctx, self.type))
end

Type.getUnqualifiedCanonicalTypeName = function(self)
	return strToLua(lppclang.getUnqualifiedCanonicalTypeName(self.ast.ctx, self.type))
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

Lexer = {}
Lexer.__index = Lexer

Lexer.new = function(ctx, l)
	assert(ctx and l)

	local o = setmetatable({}, Lexer)
	o.ctx = ctx
	o.lex = l
	o.curt = nil
	return o
end

Lexer.next = function(self)
	self.curt = lppclang.lexerNextToken(self.lex)
	if self.curt ~= nil then
		if self.section and self.consume then
			self.section:consumeFromBeginning(#self:curtRaw())
		end
		if not self.keep_whitespace and self:at(Lexer.whitespace) then
			return self:next()
		end
		return true
	end
end

Lexer.curtRaw = function(self)
	return strToLua(lppclang.tokenGetRaw(self.ctx, self.lex, self.curt))
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

local loaded = false

return
{

	init = function(lpp_in, libpath)
		if loaded then return true end

		lpp = lpp_in
		lppclang = ffi.load(libpath)

		-- patch lpp with the clang module
		lpp.clang = clang

		loaded = true

		return true
	end

}
