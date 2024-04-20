#include "lex.h"
#include "logger.h"

#include "ctype.h"

// ==============================================================================================================
/* ------------------------------------------------------------------------------------------------ Lexer helpers
 */
u8 Lexer::current()     { return *cursor; }
u8 Lexer::next()        { return *(cursor + 1); }
b8 Lexer::at(u8 c)      { return current() == c; }
b8 Lexer::next_at(u8 c) { return next() == c; }
b8 Lexer::eof()         { return at(0); }

b8 Lexer::at_first_identifier_char() { return isalpha(current()) || at('_'); }
b8 Lexer::at_identifier_char() { return at_first_identifier_char() || isdigit(current()); }

void Lexer::advance(s32 n)
{
	for (s32 i = 0; i < n; i++)
	{
		if (at('\n'))
		{
			line  += 1;
			column = 1;
			indentation = {cursor+1, 0};
			accumulate_indentation = true;
		}
		else
		{
			column += 1;
			if (accumulate_indentation)
			{
				if (not isspace(current()))
					accumulate_indentation = false;
				else
					indentation.len += 1;
			}
		}
		cursor += 1;
	}
}

void Lexer::skip_whitespace()
{
	while (isspace(current()))
		advance();
}


/* ------------------------------------------------------------------------------------------------ Lexer::init
 */
b8 Lexer::init(Lpp* lpp_, u8* stream_, str filename_)
{
	TRACE("Lexer::init() with file '", filename_, "'\n");
	SCOPED_INDENT;

	tokens = TokenArray::create(16);
	line = column = 1;
	stream = cursor = stream_;
	lpp = lpp_;
	filename = filename_;

	return true;
}


/* ------------------------------------------------------------------------------------------------ Lexer::run
 */
b8 Lexer::run()
{
	using enum Token::Kind;

	TRACE("Lexer::run()\n");
	SCOPED_INDENT;

	Token t = {};
	
	auto reset_token = [&t, this]()
	{
		t.kind = Eof;
		t.line = line;
		t.column = column;
		t.raw.s = cursor;
		t.indentation = {};
	};

	auto finish_token = [&t, reset_token, this](Token::Kind kind, s32 len_offset = 0)
	{
		s32 len = cursor - t.raw.s;
		if (len) // this might be fine in all cases, and ignoring the offset, but im not sure 
		{
			t.kind = kind;
			t.raw.len = len + len_offset;
			t.indentation = indentation;
			tokens.push(t);
		}

		reset_token();
	};

	reset_token();

	for (;;)
	{
		skip_whitespace();

		u8* before_scan = cursor;

		while (not at('@') and
			   not at('$') and 
			   not eof())
			advance();

		// if (cursor != before_scan)
		finish_token(Subject);

		if (eof())
			break;

		switch (current())
		{
			case '$': {
				advance();
				if (at('$'))
				{
					advance();
					if (at('$'))
					{
						advance();
						reset_token();
						// this is a lua block, scan until we hit another 3 $ in a row
						for (;;)
						{
							if ((advance(), at('$')) and
								(advance(), at('$')) and
								(advance(), at('$')))
							{
								advance(); 
								finish_token(LuaBlock, -3);
								break;
							}
							
							if (eof())
								return error_at(t.line, t.column, "unexpected eof while consuming lua block (eg. code that starts with '$$$')");
						}
					}
					else
					{
						// dont handle this case for now
						// TODO(sushi) decide what should be done in this case later on
						return error_here("two of '$' in a row is unrecognized!");
					}
				}
				else if (at('('))
				{
					// TODO(sushi) idrk if we'd wanna support whitespace between the $ and ( here but for now i wont allow it for simplicity
					advance();
					reset_token();

					while (not at(')') and not eof())
						advance();

					if (eof())
						return error_at(t.line, t.column, "unexpected eof while consuming inline lua (eg. code of the form $(...))");

					advance();
					finish_token(LuaInline, -1);
				}
				else
				{
					reset_token();
					while (not at('\n') and not eof())
						advance();

					finish_token(LuaLine);

					if (not eof())
						advance();

					// remove whitespace before lua lines to preserve formatting 
					if (tokens.len() > 0)
					{
						Token* last = tokens.arr + tokens.len() - 2;
						while (isspace(last->raw.s[last->raw.len-1]))
							last->raw.len -= 1;
					}
				}

			} break;

			case '@': {

				advance();
				finish_token(MacroSymbol);

				skip_whitespace();
				reset_token();

				if (not at_first_identifier_char())
					return error_here("expected an identifier of a macro after '@'");

				while (at_identifier_char())
					advance();

				finish_token(MacroIdentifier);
				
				skip_whitespace();

				switch (current())
				{
					case '(': {
						advance();
						skip_whitespace();

						// if we find an empty tuple just move on w/o making a token
						if (at(')'))
						{
							advance();
							reset_token();
							break;
						}

						for (;;)
						{
							reset_token();
							
							while (not at(',') and 
								   not at(')') and 
								   not eof())
								advance();

							if (eof())
								return error_at(t.line, t.column, "unexpected eof while consuming macro arguments");

							finish_token(MacroArgumentTupleArg);

							if (at(')'))
								break;

							advance();
							skip_whitespace();
						}

						advance();
						reset_token();
					} break;
				}
			} break;
		}
	}
	
	tokens.push({});

	return true;
}
