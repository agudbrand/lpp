#include "lex.h"
#include "logger.h"

#include "ctype.h"
#include "assert.h"

// ==============================================================================================================
/* ------------------------------------------------------------------------------------------------ Lexer helpers
 */
u32 Lexer::current()    { return cursor_codepoint; }
u8* Lexer::currentptr() { return cursor.bytes - cursor_codepoint.advance; }

b8 Lexer::at(u8 c) { return current() == c; }
b8 Lexer::eof()    { return at(0) || !cursor_codepoint.is_valid(); }

b8 Lexer::at_first_identifier_char() { return isalpha(current()) || at('_'); }
b8 Lexer::at_identifier_char() { return at_first_identifier_char() || isdigit(current()); }

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
				longjmp(err_handler, 0);
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
			indentation = {cursor.bytes, 0};
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
	while (isspace(current()))
		advance();
}

/* ------------------------------------------------------------------------------------------------ Lexer::init
 */
b8 Lexer::init(io::IO* input_stream, str stream_name_, Logger::Verbosity verbosity)
{
    assert(input_stream);

    logger.init("lpp.lexer"_str, verbosity);

    TRACE("initializing with input stream '", stream_name, "'\n");
	SCOPED_INDENT;

    in = input_stream;
	line = 1;
    column = 0;
    stream_name = stream_name_;

    stream_buffer.open();
    advance();

	return true;
}

/* ------------------------------------------------------------------------------------------------ Lexer::deinit
 */
void Lexer::deinit()
{
    stream_buffer.close();
	stream_name = {};
	in = nullptr;
}

/* ------------------------------------------------------------------------------------------------ Lexer::next_token
 */
Token Lexer::next_token()
{
	using enum Token::Kind;

	TRACE("next_token\n");
	SCOPED_INDENT;

    if (eof())
        return {Eof};

    skip_whitespace();

	// Having to deal with state like this is kinda scuffed, but I want to avoid making 
	// tokens for anything except lpp syntax. This is entirely due to the lexer being 
	// incremental and also giving individual tokens for the macro symbol, identifier, 
	// and arguments. If the entire macro were to be a single token it wouldn;t have to be
	// like this, but i prefer keeping the lexing here and not having to lex a string in the
	// parser.
	switch (last_token_kind)
	{
		case MacroSymbol:
			if (!consume_macro_identifier())
				return Token::invalid();
			return curt;

		case MacroIdentifier:
			skip_whitespace();
			switch (current())
			{
				case '(':
					advance();
					skip_whitespace();

					// if we find an empty tuple just move on w/o making a token
					if (at(')'))
					{
						advance();
						if (!consume_document_text())
							return Token::invalid();
					}
					if (!consume_macro_tuple_argument())
						return Token::invalid();
					break;

				default:
					if (!consume_document_text())
						return Token::invalid();
					break;
			}
			break;

		case MacroArgumentTupleArg:
			if (!consume_macro_tuple_argument())
				return Token::invalid();
			break;
	
		default:
			if (at('@')) // - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - @
			{
				finish_curt(MacroSymbol);
				advance();
			}
			else if (at('$')) // - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - $
			{
				if (!consume_lua_code())
					return Token::invalid();
			}
			else
			{
				if (!consume_document_text())
					return Token::invalid();
			}
			break;
	}

	return curt;
}

/* ------------------------------------------------------------------------------------------------ Lexer::reset_token
 */
void Lexer::init_curt()
{
	curt.kind = Token::Kind::Eof;
	curt.line = line;
	curt.column = column;
	curt.source_location = currentptr() - stream_buffer.buffer;
	curt.indentation_length = -1;
}

/* ------------------------------------------------------------------------------------------------ Lexer::finish_token
 */
void Lexer::finish_curt(Token::Kind kind, s32 len_offset)
{
	s32 len = (currentptr() - stream_buffer.buffer) - curt.source_location;
	curt.kind = kind;
	curt.length = len + len_offset;
	curt.indentation_location = 0;
	last_token_kind = kind;
	
}

/* ------------------------------------------------------------------------------------------------ Lexer::consume_document_text
 */
b8 Lexer::consume_document_text()
{
	init_curt();

	while (not at('@') and
		   not at('$') and 
		   not eof())
		advance();

	finish_curt(Subject);

	return true;
}

/* ------------------------------------------------------------------------------------------------ Lexer::consume_lua_code
 */
b8 Lexer::consume_lua_code()
{
	advance();
	if (at('$'))
	{
		advance();
		if (at('$'))
		{
			advance();
			init_curt();
			for (;;)
			{
				if ((advance(), at('$')) and
					(advance(), at('$')) and
					(advance(), at('$')))
				{
					advance();
					finish_curt(LuaBlock, -3);
					return true;
				}

				if (eof())
					return error_at(curt.line, curt.column, "unexpected eof while consuming lua block");
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
		advance();
		init_curt();

		while (not at(')') and not eof())
			advance();

		if (eof())
			return error_at(curt.line, curt.column, "unexpected eof while consuming inline lua");

		advance();
		finish_curt(LuaInline, -1);
		return true;
	}
	else
	{
		init_curt();
		while (not at('\n') and not eof())
			advance();

		finish_curt(LuaLine);

		if (not eof())
			advance();

		return true;
	}

}

/* ------------------------------------------------------------------------------------------------ Lexer::consume_macro_identifier
 */
b8 Lexer::consume_macro_identifier()
{
	init_curt();

	if (not at_first_identifier_char())
		return error_here("expected an identifier of a macro after '@'");

	while (at_identifier_char())
		advance();

	finish_curt(MacroIdentifier);

	return true;
}

/* ------------------------------------------------------------------------------------------------ Lexer::consume_macro_tuple_argument
 */
b8 Lexer::consume_macro_tuple_argument()
{
	if (at(')'))
	{
		advance();
		return consume_document_text();
	}

	init_curt();

	while (not at(',') and 
		   not at(')') and 
		   not eof())
		advance();

	if (eof())
		return error_at(curt.line, curt.column, "unexpected eof while consuming macro arguments");

	finish_curt(MacroArgumentTupleArg);

	return true;
}
