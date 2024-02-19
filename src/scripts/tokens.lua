-- load common hashing function
local ffi = require "ffi"
ffi.cdef [[
	typedef struct str {
		const char* s;
		int32_t len;
	} str;

	uint64_t hash_string(str s);
]]
local C = ffi.C

local tokens_varlen = {
	"identifier",
	"literal_integer",
	"literal_float",
	"literal_double",
	"literal_string",
	"comment",
	"whitespace",
}

local tokens_c_keywords = {
	"auto",
	"break",
	"case",
	"char",
	"const",
	"continue",
	"default",
	"do",
	"double",
	"else",
	"enum",
	"extern",
	"float",
	"for",
	"goto",
	"if",
	"inline",
	"int",
	"long",
	"register",
	"restrict",
	"return",
	"short",
	"signed",
	"sizeof",
	"static",
	"struct",
	"switch",
	"typedef",
	"union",
	"unsigned",
	"void",
	"volatile",
	"while",
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

--	lpp tokens

local tokens_lpp = {
	"lpp_lua_line",
	"lpp_lua_block",
	"lpp_lua_inline",
	"lpp_lua_macro",
	"lpp_lua_macro_argument",
}

-- generate enum

local header = [[
typedef enum {
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

]]

-- generate hashmaps

header = header..[[
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
static const u64 kwmap_c_count = ]]..#tokens_c_keywords+#tokens_c_new_keywords..[[;

static const kwvk kwmap_cpp[] =
{
]]

for _,v in ipairs(tokens_cpp_keywords) do
    header = header.."\t{"..tostring(C.hash_string(strtype(v, #v)))..", ".."tok_"..v.."},\n"
end

header = header..[[
};
static const u64 kwmap_cpp_count = ]]..#tokens_cpp_keywords..";"

local f = io.open("src/generated/tokens.h", "w")

if not f then
    error("failed to open 'src/generated/tokens.h'")
end

f:write(header)
f:close()
