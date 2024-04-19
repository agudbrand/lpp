-- the token enum
local enum_header_path    = "src/generated/token.enum.h"
-- mapping of keyword string hashes to token kinds for
-- use by the lexer
local map_header_path     = "src/generated/token.map.h"
-- array of strings indexable by TokenKind
local strings_header_path = "src/generated/token.strings.h"
-- a mapping of strings to token kinds for use in metaprograms.
-- this is what allows us to do token.kind == "..."
local string_mapping_path = "src/generated/tokens.stringmap.h"

local CGen = require "src.cgen"

local kind_enum = CGen.new()
kind_enum:begin_enum("Kind")

local trie_func = CGen.new()
trie_func:begin_function("Token::Kind", "keyword_or_identifier", "str s")

local tokens_varlen =
{
	"Identifier",
	"LiteralInteger",
	"LiteralFloat",
	"LiteralDouble",
	"LiteralString",
	"Comment",
	"Whitespace",
}

local tokens_c_keywords =
{
	{ "auto",     "Auto"     },
	{ "break",    "Break"    },
	{ "case",     "Case"     },
	{ "char",     "Char"     },
	{ "const",    "Const"    },
	{ "continue", "Continue" },
	{ "default",  "Default"  },
	{ "do",       "Do"       },
	{ "double",   "Double"   },
	{ "else",     "Else"     },
	{ "enum",     "Enum"     },
	{ "extern",   "Extern"   },
	{ "float",    "Float"    },
	{ "for",      "For"      },
	{ "goto",     "Goto"     },
	{ "if",       "If"       },
	{ "inline",   "Inline"   },
	{ "int",      "Int"      },
	{ "long",     "Long"     },
	{ "register", "Register" },
	{ "restrict", "Restrict" },
	{ "return",   "Return"   },
	{ "short",    "Short"    },
	{ "signed",   "Signed"   },
	{ "sizeof",   "Sizeof"   },
	{ "static",   "Static"   },
	{ "struct",   "Struct"   },
	{ "switch",   "Switch"   },
	{ "typedef",  "Typedef"  },
	{ "union",    "Union"    },
	{ "unsigned", "Unsigned" },
	{ "void",     "Void"     },
	{ "volatile", "Volatile" },
	{ "while",    "While"    },
}

local tokens_c_new_keywords = {
	"_Aliasas",
	"_Alignof",
	"_Atomic",
	"_Bool",
	"_Complex",
	"_Generic",
	"_Imaginary",
	"_Noreturn",
	"_Static_assert",
	"_Thread_local",
}

local tokens_cpp_keywords = {
	"alignas",
	"alignof",
	"bool",
	"catch",
	"class",
	"concept",
	"const_cast",
	"consteval",
	"constexpr",
	"co_await",
	"co_return",
	"co_yield",
	"decltype",
	"delete",
	"dynamic_cast",
	"explicit",
	"export",
	"false",
	"friend",
	"mutable",
	"namespace",
	"new",
	"noexcept",
	"nullptr",
	"operator",
	"private",
	"protected",
	"public",
	"reinterpret_cast",
	"requires",
	"static_assert",
	"static_cast",
	"template",
	"this",
	"thread_local",
	"throw",
	"true",
	"try",
	"typeid",
	"typename",
	"using",
	"virtual",
	"wchar_t",
}

local tokens_punctuation = {
	{"explanation_mark",         "!"  },
	{"percent",                  "%"  },
	{"caret",                    "^"  },
	{"ampersand",                "&"  },
	{"asterisk",                 "*"  },
	{"parenthesis_left",         "("  },
	{"parenthesis_right",        ")"  },
	{"minus",                    "-"  },
	{"minus_double",             "--" },
	{"plus",                     "+"  },
	{"plus_double",              "++" },
	{"equal",                    "="  },
	{"equal_double",             "==" },
	{"explanation_mark_equal",   "!=" },
	{"brace_left",               "{"  },
	{"brace_right",              "}"  },
	{"vertical_bar",             "|"  },
	{"tilde",                    "~"  },
	{"square_left",              "["  },
	{"square_right",             "]"  },
	{"backslash",                "\\" },
	{"semicolon",                ";"  },
	{"quote_single",             "'"  },
	{"colon",                    ":"  },
	{"quote_double",             "\"" },
	{"angle_left",               "<"  },
	{"angle_right",              ">"  },
	{"question_mark",            "?"  },
	{"comma",                    ","  },
	{"period",                   "."  },
	{"slash",                    "/"  },
	{"pound",                    "#"  },
	{"angle_left_double",        "<<" },
	{"angle_right_double",       ">>" },
	{"angle_left_equal",         "<=" },
	{"angle_right_equal",        ">=" },
	{"ampersand_double",         "&&" },
	{"vertical_bar_double",      "||" },
	{"ellipses",                 "..."},
	{"asterisk_equal",           "*=" },
	{"slash_equal",              "/=" },
	{"percent_equal",            "%=" },
	{"plus_equal",               "+=" },
	{"minus_equal",              "-=" },
	{"arrow",                    "->" },
	{"angle_left_double_equal",  "<<="},
	{"angle_right_double_equal", ">>="},
	{"ampersand_equal",          "&=" },
	{"caret_equal",              "^=" },
	{"vertical_bar_equal",       "|=" },
	{"tilde_equal",              "~=" },
}

local tokens_lpp = {
	"lpp_lua_line",
	"lpp_lua_block",
	"lpp_lua_inline",
	"lpp_lua_macro",
	"lpp_lua_macro_argument",
}


-- [[
--
--		Generate enum header
--
-- ]]


local header = [[
#ifndef _lpp_generated_token_enum_h
#define _lpp_generated_token_enum_h

typedef enum {
	tok_NULL = 0,
]]

for _,v in ipairs(tokens_varlen) do
    header = header.."\ttok_"..v..",\n"
end

for _,v in ipairs(tokens_c_keywords) do
    header = header.."\ttok_"..v..",\n"
end

for _,v in ipairs(tokens_c_new_keywords) do
    header = header.."\ttok_"..v..",\n"
end

for _,v in ipairs(tokens_cpp_keywords) do
    header = header.."\ttok_"..v..",\n"
end

for _,v in ipairs(tokens_punctuation) do
    header = header.."\ttok_"..v[1]..",\n"
end

for _,v in ipairs(tokens_lpp) do
    header = header.."\ttok_"..v..",\n"
end

header = header..[[
} TokenKind;

#endif // _lpp_generated_token_enum_h
]]

local enum_header = io.open(enum_header_path, "w")

if not enum_header then
	error("failed to open enum header at path '"..enum_header_path.."'")
end

enum_header:write(header)
enum_header:close()


-- [[
--
--		Generate keyword maps header
--
-- ]]


header = ""

header = header..[[
#ifndef _lpp_generated_token_map_h
#define _lpp_generated_token_map_h

#include "common.h"

typedef struct kwvk 
{
    u64 key;
    TokenKind kind;
} kwvk;

static const kwvk kwmap_c[] = 
{
]]

local strtype = ffi.typeof("str")

for _,v in ipairs(tokens_c_keywords) do
    header = header.."\t{"..tostring(C.hash_string(strtype(v, #v)))..", ".."tok_"..v.."},\n"
end

for _,v in ipairs(tokens_c_new_keywords) do
    header = header.."\t{"..tostring(C.hash_string(strtype(v, #v)))..", ".."tok_"..v.."},\n"
end

header = header..[[
};
static const u64 kwmap_c_count = sizeof(kwmap_c)/sizeof(kwmap_c[0]);

static const kwvk kwmap_cpp[] =
{
]]

for _,v in ipairs(tokens_cpp_keywords) do
    header = header.."\t{"..tostring(C.hash_string(strtype(v, #v)))..", ".."tok_"..v.."},\n"
end

header = header..[[
};
static const u64 kwmap_cpp_count = sizeof(kwmap_cpp)/sizeof(kwmap_cpp[0]);

#endif // _lpp_generated_token_map_h
]]

local map_header = io.open(map_header_path, "w")

if not map_header then
    error("failed to open map header for writing at '"..map_header_path.."'")
end

map_header:write(header)
map_header:close()


-- [[
--
--		Generate strings header
--
-- ]]


header = [[
#ifndef _lpp_generated_token_strings_h
#define _lpp_generated_token_strings_h

static const char* token_strings[] =
{
]]

for _,v in ipairs(tokens_varlen) do
    header = header.."\t\""..v.."\",\n"
end

for _,v in ipairs(tokens_c_keywords) do
    header = header.."\t\""..v.."\",\n"
end

for _,v in ipairs(tokens_c_new_keywords) do
    header = header.."\t\""..v.."\",\n"
end

for _,v in ipairs(tokens_cpp_keywords) do
    header = header.."\t\""..v.."\",\n"
end

for _,v in ipairs(tokens_punctuation) do
    header = header.."\t\""..v[1].."\",\n"
end

for _,v in ipairs(tokens_lpp) do
    header = header.."\t\""..v.."\",\n"
end

header = header..[[
};

#endif // _lpp_generated_token_strings_h
]]

local strings_header = io.open(strings_header_path, "w")

if not strings_header then
	error("failed to open strings header for writing at '"..strings_header_path.."'")
end

strings_header:write(header)
strings_header:close()


-- [[
--
--		Generate string mapping
--
-- ]]


header = [[
#ifndef _lpp_generated_token_string_map_h
#define _lpp_generated_token_string_map_h

#include "common.h"
#include "token.enum.h"

typedef struct strtoken {
	u64 key;
	TokenKind kind;
} strtoken;

static const strtoken strtokenmap[] =
{
]]

for _,v in ipairs(tokens_varlen) do
    header = header.."\t{"..tostring(C.hash_string(strtype(v, #v)))..", ".."tok_"..v.."}, \n"
end

for _,v in ipairs(tokens_c_keywords) do
    header = header.."\t{"..tostring(C.hash_string(strtype(v, #v)))..", ".."tok_"..v.."},\n"
end

for _,v in ipairs(tokens_c_new_keywords) do
    header = header.."\t{"..tostring(C.hash_string(strtype(v, #v)))..", ".."tok_"..v.."},\n"
end

for _,v in ipairs(tokens_cpp_keywords) do
    header = header.."\t{"..tostring(C.hash_string(strtype(v, #v)))..", ".."tok_"..v.."},\n"
end

for _,v in ipairs(tokens_punctuation) do
    header = header.."\t{"..tostring(C.hash_string(strtype(v[1], #v[1])))..", ".."tok_"..v[1].."},\n"
    header = header.."\t{"..tostring(C.hash_string(strtype(v[1], #v[2])))..", ".."tok_"..v[1].."},\n"
end

header = header..[[
};
static const u64 strtokenmap_len = sizeof(strtokenmap)/sizeof(strtokenmap[0]);

#endif // _lpp_generated_token_string_map_h
]]

local string_mapping_header = io.open(string_mapping_path, "w")

if not string_mapping_header then
	error("failed to open string mapping header for writing at '"..string_mapping_path.."'")
end

string_mapping_header:write(header)
string_mapping_header:close()
