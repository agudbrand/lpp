
local ffi = require "ffi"
ffi.cdef [[
		
		
	typedef enum 
	{
		tok_identifier,
		tok_literal_integer,
		tok_literal_float,
		tok_literal_double,
		tok_literal_string,
		tok_comment,
		tok_whitespace,

		/*  C keywords
		 */
		tok_auto,
		tok_break,
		tok_case,
		tok_char,
		tok_const,
		tok_continue,
		tok_default,
		tok_do,
		tok_double,
		tok_else,
		tok_enum,
		tok_extern,
		tok_float,
		tok_for,
		tok_goto,
		tok_if,
		tok_inline,
		tok_int,
		tok_long,
		tok_register,
		tok_restrict,
		tok_return,
		tok_short,
		tok_signed,
		tok_sizeof,
		tok_static,
		tok_struct,
		tok_switch,
		tok_typedef,
		tok_union,
		tok_unsigned,
		tok_void,
		tok_volatile,
		tok_while,

		/*  newer C stuff
		 */
		tok__Aliasas,
		tok__Alignof,
		tok__Atomic,
		tok__Bool,
		tok__Complex,
		tok__Generic,
		tok__Imaginary,
		tok__Noreturn,
		tok__Static_assert,
		tok__Thread_local,

		/*  C++ keywords
		 */
		tok_alignas,
		tok_alignof,
		tok_bool,
		tok_catch,
		tok_class,
		tok_concept,
		tok_const_cast,
		tok_consteval,
		tok_constexpr,
		tok_co_await,
		tok_co_return,
		tok_co_yield,
		tok_decltype,
		tok_delete,
		tok_dynamic_cast,
		tok_explicit,
		tok_export,
		tok_false,
		tok_friend,
		tok_mutable,
		tok_namespace,
		tok_new,
		tok_noexcept,
		tok_nullptr,
		tok_operator,
		tok_private,
		tok_protected,
		tok_public,
		tok_reinterpret_cast,
		tok_requires,
		tok_static_assert,
		tok_static_cast,
		tok_template,
		tok_this,
		tok_thread_local,
		tok_throw,
		tok_true,
		tok_try,
		tok_typeid,
		tok_typename,
		tok_using,
		tok_virtual,
		tok_wchar_t,

		// TODO(sushi) compiler specific keywords
		
		/*  Punctuators
		 */
		tok_explanation_mark,
		tok_percent,
		tok_caret,
		tok_ampersand,
		tok_asterisk,
		tok_parenthesis_left,
		tok_parenthesis_right,
		tok_minus,
		tok_minus_double,
		tok_plus,
		tok_plus_double,
		tok_equal,
		tok_equal_double,
		tok_explanation_mark_equal,
		tok_brace_left,
		tok_brace_right,
		tok_vertical_bar,
		tok_tilde,
		tok_square_left,
		tok_square_right,
		tok_backslash,
		tok_semicolon,
		tok_quote_single,
		tok_colon,
		tok_quote_double,
		tok_angle_left,
		tok_angle_right,
		tok_question_mark,
		tok_comma,
		tok_period,
		tok_slash,
		tok_pound,
		tok_angle_left_double,
		tok_angle_right_double,
		tok_angle_left_equal,
		tok_angle_right_equal,
		tok_ampersand_double,
		tok_vertical_bar_double,
		tok_ellipses,
		tok_asterisk_equal,
		tok_slash_equal,
		tok_percent_equal,
		tok_plus_equal,
		tok_minus_equal,
		tok_arrow,
		tok_angle_left_double_equal,
		tok_angle_right_double_equal,
		tok_ampersand_equal,
		tok_caret_equal,
		tok_vertical_bar_equal,
		tok_tilde_equal,

		/*	lpp tokens
		 */
		tok_lpp_lua_line,
		tok_lpp_lua_block,
		tok_lpp_lua_inline,
		tok_lpp_lua_macro,
		tok_lpp_lua_macro_argument,
	} TokenKind;

    typedef struct TokenReturn {
        char* raw_str;
        int   raw_len;

		TokenKind kind;

		int line;
		int column;
		char* file_name;
    } TokenReturn;

	TokenReturn get_token(void* ctx, int idx);
]]
local C = ffi.C

local M = {}

local __LPP = {}
M.__LPP = __LPP

do -- shallow copy the global table into the meta env
	for k,v in pairs(_G) do
		M[k] = v
	end
end

local result = ""
local tok_idx = 0
local token_array = {}

__LPP.
current_token = function()
	local tok = C.get_token(__LPP.context, tok_idx)
	local out = {}
	out.file_name = ffi.string(tok.file_name)
	out.kind = tok.kind
	out.line = tok.line
	out.column = tok.column
	out.raw = ffi.string(tok.raw_str, tok.raw_len)
	return out
end

__LPP.
next_token = function()
	tok_idx = tok_idx + 1
	return __LPP.current_token()
end

local function append_result(s)
	if type(s) ~= "string" then
		error("append_result() got "..type(s).." instead of a string!", 3)
	end

	result = result .. s
end

M.
__C = function(str)
	append_result(str)
end

M.
__VAL = function(x)
	append_result(tostring(x))
end

M.
__FINAL = function()
	return result
end

M.
__TOKENCONTEXT = function(idx)
    tok_idx = idx
end

M.
__MACRO = function(result)
	if not result then
		error("Lua macro did not result in a string, got: "..type(result), 2)
	end
	return result
end

return M
