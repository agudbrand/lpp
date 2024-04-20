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

Logger log;

/* ------------------------------------------------------------------------------------------------ Lpp::run
 */
b8 Lpp::run()
{
	log.verbosity = Logger::Verbosity::Trace;

	TRACE("Lpp::run()\n");

	FILE* file = fopen((char*)input_file_name.s, "r");
	if (!file)
	{
		error(input_file_name, 0, 0, "failed to open file");
		return false;
	}

	str buffer = {};

	fseek(file, 0, SEEK_END);
	buffer.len = ftell(file);
	fseek(file, 0, SEEK_SET);
	
	buffer.s = (u8*)mem.allocate(buffer.len);
	defer { mem.free(buffer.s); };

	if (!fread(buffer.s, buffer.len, 1, file))
	{
		error(input_file_name, 0, 0, "failed to read file");
		return false;
	}

	if (!lexer.init(this, buffer.s, input_file_name))
		return false;

	if (!lexer.run())
		return false;

	for (s32 i = 0; i < lexer.tokens.len(); i++)
	{
		printv(Token::kind_string(lexer.tokens[i].kind), "\n");
	}
	
	dstr mp = dstr::create();

	auto build_subject = [&mp](u8* s, s32 len)
	{
		mp.append("__SUBJECT(\""_str);

		for (s32 i = 0; i < len; i++)
		{
			u8 c = s[i];

			if (iscntrl(c))
				mp.appendv("\\", c);
			else if (c == '"')
				mp.appendv("\\", '"');
			else if (c == '\\')
				mp.appendv("\\\\");
			else
				mp.append((char)c);
		}

		mp.append("\")\n");
	};

	auto build_metalua = [&mp](u8* s, s32 len)
	{
		mp.append(str{s, len});
	};

	auto build_metalua_line = [&mp](u8* s, s32 len)
	{
		mp.appendv(str{s, len}, "\n");
	};

	auto curt = TokenIterator::from(lexer.tokens);
	
	for (;;)
	{
		using enum Token::Kind;

		if (curt->kind == Eof)
			break;

		switch (curt->kind)
		{
			case Subject: {
				mp.append("__SUBJECT(\""_str);

				for (u8 c : curt->raw)
				{
					if (iscntrl(c))
						mp.appendv("\\"_str, c);
					else if (c == '"')
						mp.appendv("\\\""_str);
					else if (c == '\\')
						mp.appendv("\\\\"_str);
					else
						mp.append((char)c);
				}

				mp.append("\")\n"_str);
				curt.next();
			} break;

			case LuaLine: 
				mp.appendv(curt->raw, "\n");
				curt.next();
				break;

			case LuaBlock:
				mp.append(curt->raw);
				curt.next();
				break;

			case LuaInline:
				mp.appendv("__SUBJECT(__VAL("_str, curt->raw, "))\n"_str);
				curt.next();
				break;

			case MacroSymbol: {
				mp.appendv("__SET_MACRO_TOKEN_INDEX(", curt.curt - lexer.tokens.arr, ")\n");
				mp.append("__SUBJECT(__MACRO("_str);
				
				curt.next(); // identifier
				mp.append(curt->raw);

				if (curt.next() && curt->kind == MacroArgumentTupleArg)
				{
					mp.append('(');
					if (curt->kind == MacroArgumentTupleArg)
					{
						for (;;)
						{
							mp.appendv('"', curt->raw, '"');

							if (curt.next())
							{
								if (curt->kind != MacroArgumentTupleArg)
									break;

								mp.append(',');
							}
							else
								break;
						}
					}
					mp.append(')');
				}

				mp.append("))\n"_str);
			} break;
		}
	}


	mp.append("return __FINAL()\n");

	print(mp);
	print("\n\n-------------------\n\n");

	lua_State* L = lua_open();
	luaL_openlibs(L);

	if (luaL_loadbuffer(L, (char*)mp.s, mp.len, (char*)input_file_name.s))
	{
		ERROR("failed to load metaprogram:\n", lua_tostring(L, -1), "\n");
		return false;
	}

	if (luaL_loadfile(L, "src/lpp/metaenv.lua"))
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

	printv(lua_tostring(L, -1), "\n");

	return true;
}

extern "C"
{

str get_token_indentation(Lpp* lpp, s32 idx)
{
	return lpp->lexer.tokens[idx].indentation;
}

}
