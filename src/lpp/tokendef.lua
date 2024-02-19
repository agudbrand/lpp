--[[
	Definition of C tokens. 
	This generates the TokenKind enum and the hashmap from
	strings to these enum values.

	This file is loaded and executed by tokendef.c.
--]]


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

-- load hash function
local ffi = require "ffi"
ffi.cdef [[
	typedef struct str {
		uint8_t* s;
		int32_t len;
	} str;

	uint64_t hash_string(str s);
]]

-- generate keyword maps 
