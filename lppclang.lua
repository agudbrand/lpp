--
--
-- lpp extension providing access to clang.
--
--

-- load the generated cdefs file and give them to ffi

local ffi = require "ffi"

ffi.cdef [[
typedef enum DeclKind
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
typedef struct Context Context;
typedef struct Decl Decl;
typedef struct Type Type;
typedef struct DeclIter DeclIter;
typedef struct ParamIter ParamIter;
typedef struct FieldIter FieldIter;
typedef struct EnumIter EnumIter;
Context* createContext();
void destroyContext(Context*);
b8 createASTFromString(Context* ctx, str s);
Decl* getTranslationUnitDecl(Context* ctx);
DeclIter* createDeclIter(Context* ctx, Decl* decl);
Decl* getNextDecl(DeclIter* iter);
DeclKind getDeclKind(Decl* decl);
str getDeclName(Decl* decl);
Type* getDeclType(Decl* decl);
Type* getTypeDeclType(Context* ctx, Decl* decl);
Type* getFunctionReturnType(Decl* decl);
b8 functionHasBody(Decl* decl);
u32 getFunctionBodyBegin(Decl* decl);
u32 getFunctionBodyEnd(Decl* decl);
b8 tagHasBody(Decl* decl);
u32 getTagBodyBegin(Decl* decl);
u32 getTagBodyEnd(Decl* decl);
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

-- api used by lpp user
local clang = {}

local AST,
      Decl,
	  Type,
	  Function,
	  DeclIter,
	  ParamIter,
	  FieldIter,
	  EnumIter

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
	local iter = lppclang.createDeclIter(self.ast.ctx, self.d)
	if iter == nil then
		return false
	end

	return DeclIter.new(iter)
end

Decl.name = function(self)
	local n = lppclang.getDeclName(self.decl)
	return ffi.string(n.s, n.len)
end

Decl.type = function(self)
	local t = lppclang.getDeclType(self.decl)
	return Type.new(self.ast, t)
end

Decl.asFunction = function(self)
	if lppclang.getDeclType(self.decl) == lppclang.DeclKind_Function then
		return Function.new(self.ast, self.decl)
	end
end


clang.parseString = function(str)
	local ctx = lppclang.createContext()
	
	if ctx == nil then
		error("failed to create lppclang context", 2)
	end

	ffi.gc(ctx, function(self) -- clean up context when no more references to it remain
		lppclang.destroyContext(self)
	end)

	if lppclang.createASTFromString(ctx, make_str(str)) == 0 then
		error("failed to create AST from string", 2)
	end

	return ctx
end


return {

	init = function(lpp_in, libpath)
		lpp = lpp_in
		lppclang = ffi.load(libpath)

		-- patch lpp with the clang module
		lpp.clang = clang

		return true
	end

}
