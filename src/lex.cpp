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

    TRACE("initializing with input stream ", (void*)input_stream, "\n");
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
}

/* ------------------------------------------------------------------------------------------------ Lexer::next_token
 */
Token Lexer::next_token()
{
	using enum Token::Kind;

	TRACE("next_token\n");
	SCOPED_INDENT;

	Token t = {};
	
	auto reset_token = [&t, this]()
	{
		t.kind = Eof;
		t.line = line;
		t.column = column;
        t.source_location = currentptr() - stream_buffer.buffer;
		t.indentation_length = -1;
	};

	auto finish_token = [&t, reset_token, this](Token::Kind kind, s32 len_offset = 0)
	{
		s32 len = (currentptr() - stream_buffer.buffer) - t.source_location;
        t.kind = kind;
        t.length = len + len_offset;
        t.indentation_location = 0;
	};

	reset_token();

    if (eof())
        return {Eof};

    skip_whitespace();

    if (at('@')) // - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - @
    {
        DEBUG("found '@'\n");

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
                TRACE("macro uses tuple parameter\n");
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
    }
    else if (at('$')) // - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - $
    {
        DEBUG("found '$'\n");
        advance();
        if (at('$'))
        {
            advance();
            if (at('$'))
            {
                advance();
                reset_token();
                TRACE("found lua block\n");
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
            TRACE("found inline lua\n");
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
            TRACE("found lua line\n");
            reset_token();
            while (not at('\n') and not eof())
                advance();

            finish_token(LuaLine);

            if (not eof())
                advance();
        }
    }
    else
    {
        while (not at('@') and
               not at('$') and 
               not eof())
            advance();

        DEBUG("subject token\n");

        finish_token(Subject);
    }

	return t;
}
