#include "lpp.h"

#include "logger.h"

#include "stdio.h"
#include "ctype.h"

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
};

#include "assert.h"

/* ------------------------------------------------------------------------------------------------ Lpp::init
 */
b8 Lpp::init(io::IO* input_stream, io::IO* output_stream, Logger::Verbosity verbosity)
{
    assert(input_stream && output_stream);

    logger.init("lpp"_str, verbosity);

    TRACE("initializing with input stream ", (void*)input_stream, " and output stream ", (void*)output_stream, "\n");
    SCOPED_INDENT;

    tokens = Array<Token>::create();
    in = input_stream;
    out = output_stream;

    DEBUG("initializing lexer\n");
    if (!lexer.init(input_stream, ""_str, verbosity))
        return false;

    return true;
}

/* ------------------------------------------------------------------------------------------------ Lpp::run
 */
b8 Lpp::run()
{
    INFO("running lpp\n");

    DEBUG("opening metaprogram buffer\n");
    io::Memory metaprogram;
    metaprogram.open();

    Token curt;
    s64 idx = -1;

    auto next_token = [&idx, &curt, this]()
    {
        curt = lexer.next_token();
        idx += 1;
        tokens.push(curt);
        return curt.kind != Token::Kind::Eof;
    };
	
    INFO("starting parse\n");

    next_token();

	for (;;)
	{
		using enum Token::Kind;

        if (!curt.is_valid())
            return false;

		if (curt.kind == Eof)
			break;

		switch (curt.kind)
		{
			case Subject: {
				TRACE("placing subject: '", io::fmt::SanitizeControlCharacters(lexer.get_raw(curt)), "'\n");

				io::format(&metaprogram, "__SUBJECT(\""_str);

				for (u8 c : lexer.get_raw(curt))
				{
					if (iscntrl(c))
						io::formatv(&metaprogram, "\\"_str, c);
					else if (c == '"')
						io::format(&metaprogram, "\\\""_str);
					else if (c == '\\')
						io::format(&metaprogram, "\\\\"_str);
					else
						io::format(&metaprogram, (char)c);
				}
				metaprogram.write("\")\n"_str);
                next_token();
			} break;

			case LuaLine: 
                TRACE("placing lua line: '", io::fmt::SanitizeControlCharacters(lexer.get_raw(curt)), "'\n");
				// cleanup any whitespace preceeding this token to keep formatting correct
				io::formatv(&metaprogram, lexer.get_raw(curt), "\n");

				next_token();
				break;

			case LuaBlock:
                io::format(&metaprogram, lexer.get_raw(curt));
				next_token();
				break;

			case LuaInline:
                io::formatv(&metaprogram, "__SUBJECT(__VAL("_str, lexer.get_raw(curt), "))\n"_str);
				next_token();
				break;

			case MacroSymbol: {
                io::formatv(&metaprogram,
                        "__SET_MACRO_TOKEN_INDEX("_str, idx, ")\n",
                        "__SUBJECT(__MACRO("_str);
				
				next_token(); // identifier
                io::format(&metaprogram, lexer.get_raw(curt));

				if (next_token() && curt.kind == MacroArgumentTupleArg)
				{
                    io::format(&metaprogram, '(');
					if (curt.kind == MacroArgumentTupleArg)
					{
						for (;;)
						{
                            io::formatv(&metaprogram, '"', lexer.get_raw(curt), '"');

							if (next_token())
							{
								if (curt.kind != MacroArgumentTupleArg)
									break;

                                io::format(&metaprogram, ',');
							}
							else
								break;
						}
					}
                    io::format(&metaprogram, ')');
				}

                io::format(&metaprogram, "))\n"_str);
			} break;
		}
	}

    io::format(&metaprogram, "return __FINAL()\n");

    INFO(metaprogram.as_str(), "\n\n--------------------\n\n");

	lua_State* L = lua_open();
	luaL_openlibs(L);

	if (luaL_loadbuffer(L, (char*)metaprogram.buffer, metaprogram.len, "metaprogram"))
	{
		ERROR("failed to load metaprogram:\n", lua_tostring(L, -1), "\n");
		return false;
	}

	if (luaL_loadfile(L, "src/metaenv.lua"))
	{
		ERROR("failed to load metaevironment:\n", lua_tostring(L, -1), "\n");
		return false;
	}

	if (lua_pcall(L, 0, 2, 0))
	{
		ERROR("failed to run metaenvironment:\n", lua_tostring(L, -1), "\n");
		return false;
	}

	// the lpp table is at the top of the stack and we need to give it 
	// a handle this this context
	lua_pushstring(L, "handle");
	lua_pushlightuserdata(L, this);
	lua_settable(L, -3);
	lua_pop(L, 1);

	if (!lua_setfenv(L, 1))
	{
		ERROR("failed to set environment of metaprogram:\n", lua_tostring(L, -1), "\n");
		return false;
	}

	if (lua_pcall(L, 0, 1, 0))
	{
		ERROR("failed to run metaprogram:\n", lua_tostring(L, -1), "\n");
		return false;
	}

	size_t len;
	const char* s = lua_tolstring(L, -1, &len);

	out->write({(u8*)s, s64(len)});

	return true;
}

extern "C"
{

str get_token_indentation(Lpp* lpp, s32 idx)
{
	return ""_str; //lpp->tokens[idx].indentation;
}

}
