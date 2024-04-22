#include "lex.h"
#include "logger.h"

#include "ctype.h"

namespace json
{


/* ================================================================================================= 
 * =================================================================================== Lexer helpers
 */
u32 Lexer::current()     { return cursor_codepoint; }

b8 Lexer::at(u8 c)      { return current() == c; }
b8 Lexer::eof()         { return at(0); }

b8 Lexer::at_first_identifier_char() { return isalpha(current()) || at('_'); }
b8 Lexer::at_identifier_char() { return at_first_identifier_char() || isdigit(current()); }
b8 Lexer::at_whitespace() { return isspace(current()) != 0; }

void Lexer::advance(s32 n)
{
    auto read_stream_if_needed = [this]()
    {
        if (stream_buffer.at_end() ||
            cursor.isempty())
        {
            u8* ptr = stream_buffer.reserve(128);
            s64 bytes_read = in->read({ptr, 128});
            if (!bytes_read)
            {
                cursor_codepoint = utf8::Codepoint::invalid();
                error_here("failed to read more bytes from input stream");
                return;
            }
            stream_buffer.commit(bytes_read);
            cursor.bytes = ptr;
            cursor.len = 128;
        }
    };

    read_stream_if_needed();

	for (s32 i = 0; i < n; i++)
	{
		if (at('\n'))
		{
			line  += 1;
			column = 1;
		}
		else
		{
			column += 1;
		}

        read_stream_if_needed();
		cursor_codepoint = cursor.advance();

        if (!cursor_codepoint)
        {
            error_here("encountered invalid codepoint!");
            return;
        }
	}
}

void Lexer::skip_whitespace()
{
	while (at_whitespace())
		advance();
}

/* ------------------------------------------------------------------------------------------------ json::Lexer::init
 */
b8 Lexer::init(io::IO* input_stream, utf8::str stream_name_, Logger::Verbosity v)
{
    logger.init("json.lexer"_str, v);

    INFO("initializing with input stream ", (void*)input_stream);

    in = input_stream;
	flags = Flags::none();
	stream_name = stream_name_;
	line   = 1;
	column = 0;

    stream_buffer.open();
	advance();

	return true;
}

/* ------------------------------------------------------------------------------------------------ json::Lexer::init
 */
void Lexer::deinit()
{
	cursor = str{};
	flags = Flags::none();
}

/* ------------------------------------------------------------------------------------------------ json::Lexer::next_token
 */
Token Lexer::next_token()
{
	using enum Token::Kind;

	Token t = {};

	auto reset_token = [&t, this]()
	{
		t.line = line;
		t.column = column;
		t.raw.bytes = cursor.bytes - 1;
	};

	auto finish_token = [&t, this](Token::Kind kind)
	{
		t.raw.len = cursor.bytes - 1 - t.raw.bytes;
		t.kind = kind;
	};

	if (at_whitespace())
	{
		skip_whitespace();
		if (flags.test(Flag::ReturnWhitespaceTokens))
		{
			t.raw.len = cursor.bytes - t.raw.bytes;
			t.kind = Whitespace;
			return t;
		}
	}

	if (eof() || cursor.isempty())
		return {Eof};

	reset_token();

	switch (current())
	{
#define singleglyph(c, k) case c: advance(); finish_token(k); break;
		singleglyph('[', LeftSquare);
		singleglyph(']', RightSquare);
		singleglyph('{', LeftBrace);
		singleglyph('}', RightBrace);
		singleglyph(':', Colon);
		singleglyph(',', Comma);

		case '"': {
			// just consume the raw string here, it is processed later in parsing
			advance();
			reset_token();
			for (;;)
			{
				// skip anything after an escape 
				if (at('\\'))
					advance(2);
				if (at('"'))
					break;
				if (eof())
					return error_at(t.line, t.column, "unexpected end of file while consuming string\n");
				advance();

			}
			finish_token(String);
			advance();
		} break;

		default: {

			auto consume_number = [this]()
			{
				if (at('0'))
					advance();
				else while (isdigit(current()))
					advance();

				if (at('.'))
				{
					advance();
					if (!isdigit(current()))
					{
						error_here("expected at least one more digit after frac decimal point\n");
						return false;
					}
					while (isdigit(current()))
						advance();
				}

				if (at('e') || at('E'))
				{
					advance();
					if (at('+') || at('-'))
						advance();
					if (!isdigit(current()))
					{
						error_here("expected at least one digit for number exponent\n");
						return false;
					}
					while (isdigit(current()))
						advance();
				}

				return true;
			};

			if (current() == '-')
			{
				advance();
				if (!isdigit(current()))
					return error_here("expected a digit after '-'\n");

				if (!consume_number())
					return Token::invalid();

				finish_token(Number);
			}
			else if (isdigit(current()))
			{
				if (!consume_number())
					return Token::invalid();

				finish_token(Number);
			}
			else if (isalpha(current()))
			{
				while (isalpha(current()))
					advance();
				
				str name = {t.raw.bytes, s32(cursor.bytes - 1 - t.raw.bytes)};
				if (t.raw == "true"_str)
					finish_token(True);
				else if (t.raw == "false"_str)
					finish_token(False);
				else if (t.raw == "null"_str)
					finish_token(Null);
				else
					return error_here("unrecognized literal name '", t.raw, "'\n");
			}
		} break;
	}

	return t;
}

}
