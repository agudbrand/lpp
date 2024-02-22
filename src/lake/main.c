#include "common.h"

#include "stdio.h"
#include "stdlib.h"
#include "stdarg.h"
#include "ctype.h"

void fatal_error(const char* filename, s64 line, s64 column, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stdout, "%s", filename);
	fprintf(stdout, ":%li:%li: ", line, column);
	fprintf(stdout, "fatal_error");
	fprintf(stdout, ": ");
	vfprintf(stdout, fmt, args);
	exit(1);
}

typedef enum 
{
	tok_colon,
	tok_semicolon,
	tok_equal,
	tok_comma,
	tok_paren_left,
	tok_paren_right,
	tok_brace_left,
	tok_brace_right,
	tok_square_left,
	tok_square_right,
	tok_ellises,
	tok_dot_double,
	tok_dot,
	tok_asterisk,
	tok_caret,
	tok_percent,
	tok_tilde_equal,
	tok_plus,
	tok_minus,
	tok_less_than,
	tok_less_than_or_equal,
	tok_greater_than,
	tok_greater_than_or_equal,

	tok_not,
	tok_for,
	tok_do,
	tok_if,
	tok_else,
	tok_elseif,
	tok_then,
	tok_true,
	tok_false,
	tok_nil,
	tok_and,
	tok_or,

	tok_number,
	tok_string,

	// lake specific
	
	tok_backtick,
	tok_dollar,
	tok_colon_equal,
	tok_question_mark_equal,

} TokenKind;

typedef struct Token 
{
	TokenKind kind;

	str raw;

	s64 line;
	s64 column;
} Token;

typedef struct Lexer
{
	u8* cursor;

	s64 line;
	s64 column;
} Lexer;

/* ------------------------------------------------------------------------------------------------
 *  Helpers for getting a character relative to the current position in the stream.
 */
static u8 current(Lexer* l) { return *l->cursor; }
static u8 next(Lexer* l) { return *(l->cursor + 1); }
static u8 nextnext(Lexer* l) { return (next(l) ? *(l->cursor + 2) : 0); }

/* ------------------------------------------------------------------------------------------------
 *  Helpers for querying what the character is at the current or relative position in the stream.
 */
static b8 eof(Lexer* l) { return current(l) == 0; }
static b8 at(Lexer* l, u8 c) { return current(l) == c; }
static b8 next_at(Lexer* l, u8 c) { return next(l) == c; }
static b8 at_identifier_char(Lexer* l) { return isalnum(current(l)) || current(l) == '_'; }

/* ------------------------------------------------------------------------------------------------
 *  Central function for advancing the stream. Keeps track of line and column.
 */
static void advance(Lexer* l)
{
	if (at(l, '\n'))
	{
		l->line += 1;
		l->column = 1;
	}
	else
		l->column += 1;
	l->cursor += 1;
}

static void advance2(Lexer* l)
{
	advance(l);
	advance(l);
}

static void advance3(Lexer* l)
{
	advance(l);
	advance(l);
	advance(l);
}

/* ------------------------------------------------------------------------------------------------
 *  Helpers for advancing the stream.
 */
static void skip_line(Lexer* l)
{
	while (!at(l, '\n') && !eof(l)) advance(l);
	if (!eof(l)) advance(l);
}

static void skip_whitespace(Lexer* l, b8 make_token)
{
	while(isspace(current(l))) advance(l);
}


Token next_token(Lexer* l)
{
	Token t;
	t.line = l->line;
	t.column = l->column;

	switch (current(l))
	{
#define one_char_token(x, _kind) \
		case x: {                \
			t.kind = _kind;      \
			t.raw.len = 1;       \
			advance(l);          \
		} break;      
#define one_or_two_char_token(x0, kind0, x1, kind1) \
		case x0: {                                  \
			if (next_at(l, x1)) {                   \
				t.kind = kind1;                     \
				t.raw.len = 2;                      \
				advance2(l);                        \
			}                                       \
			else {                                  \
				t.kind = kind0;                     \
				t.raw.len = 1;                      \
				advance(l);                         \
			}                                       \
		} break;                                    
#define one_or_either_of_two_chars_token(x0, kind0, x1, kind1, x2, kind2) \
		case x0: {                                                        \
			switch (next(l)) {                                            \
				case x1: {                                                \
					t.kind = kind1;                                       \
					t.raw.len = 2;                                        \
					advance2(l);                                          \
				} break;                                                  \
				case x2: {                                                \
					t.kind = kind2;                                       \
					t.raw.len = 2;                                        \
					advance2(l);                                          \
				} break;                                                  \
				default: {                                                \
					t.kind = kind0;                                       \
					t.raw.len = 1;                                        \
					advance(l);                                           \
				} break;                                                  \
			}                                                             \
		} break;

		one_char_token(';', tok_semicolon);
		one_char_token('=', tok_equal);
		one_char_token(',', tok_comma);
		one_char_token('(', tok_paren_left);
		one_char_token(')', tok_paren_right);
		one_char_token('{', tok_brace_left);
		one_char_token('}', tok_brace_right);
		one_char_token('[', tok_square_left);
		one_char_token(']', tok_square_right);
		one_char_token('*', tok_asterisk);
		one_char_token('^', tok_caret);
		one_char_token('%', tok_percent);
		one_char_token('+', tok_plus);
		one_char_token('-', tok_minus);
		one_char_token('`', tok_backtick);
		one_char_token('$', tok_dollar);

		two_char_token('~', '=', tok_tilde_equal);
		two_char_token('?', '=', tok_question_mark_equal);

		two_char_token('i', 'f', tok_if);
		two_char_token('d', 'o', tok_do);﻿
		two_char_token('o', 'r', tok_or);

		one_or_two_char_token('.', tok_dot, '.', tok_dot_double);
		one_or_two_char_token(':', tok_colon, '=', tok_colon_equal);
		one_or_two_char_token('<', tok_less_than, '=', tok_less_than_or_equal);
		one_or_two_char_token('>', tok_greater_than, '=', tok_greater_than_or_equal);

		default: {
			if (at(l, '~')) 
			{
				if (next_at(l, '='))
				{
					t.kind = tok_tilde_equal;
				}
				else
				{
					fatal_error()
				}

			}

		} break;

	}

	return t;
}

int main() 
{
	
}﻿
