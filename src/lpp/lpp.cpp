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
	
	dstr mp = dstr::create();

	u8* end = buffer.s + buffer.len;

	u8* chunk_start = buffer.s;
	u8* scan = buffer.s;

	auto build_metac = [&mp](u8* s, s32 len)
	{
		mp.append("__C(\""_str);

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

	auto skip_whitespace = [&scan]()
	{
		while (isspace(*scan))
			scan++;
	};

	auto current = [&scan]()
	{
		return *scan;
	};

	auto at = [&scan](u8 c)
	{
		return *scan == c;
	};

	auto next_at = [&scan](u8 c)
	{
		return *(scan+1) == c; 
	};

	auto advance = [&scan]()
	{
		scan++;
	};

	auto eof = [&at]()
	{
		return at(0);
	};

	auto at_identifier_char = [&current]()
	{
		return isalnum(current()) || current() == '_';
	};

	for (;;)
	{
		while (!at('$') && !at('@') && !eof())
			scan++;
		
		if (eof())
		{
			build_metac(chunk_start, scan - chunk_start);
			break;
		}

		if (at('$'))
		{
			build_metac(chunk_start, scan - chunk_start);

			advance();

			if (at('$') && next_at('$'))
			{
				advance();
				advance();
				chunk_start = scan;

				for (;;)
				{
					while (!at('$') && !eof())
						advance();

					if (eof())
						break;

					if (at('$') && (advance(), at('$') && next_at('$')))
					{
						scan -= 1;
						break;
					}
				}

				build_metalua_line(chunk_start, scan-chunk_start);

				if (eof())
					break;

				scan += 2;
			}
			else
			{
				chunk_start = scan;
				while (!at('\n') && !eof())
					advance();
				build_metalua_line(chunk_start, scan - chunk_start);
			}

			chunk_start = scan;
		}

		if (at('@'))
		{
			advance();
			chunk_start = scan;
			skip_whitespace();
			if (!at_identifier_char())
			{
				ERROR("token following a lua macro (@) must be an identifier of a lua function\n");
				return false;
			}

			while ((at_identifier_char() || isdigit(current())) && !eof())
				advance();

			if (eof())
			{
				ERROR("unexpected end of file while consuming lua macro\n");
				return false;
			}

			build_metalua(chunk_start, scan - chunk_start);

			skip_whitespace();

			if (at('('))
			{
				advance();
				chunk_start = scan;
				mp.append('(');
				while (!eof())
				{
					mp.append('"');
					skip_whitespace();
					while (!at(',') && !at(')') && !eof()) 
						advance();

					if (eof())
					{
						ERROR("unexpected end of file while consuming lua macro arguments");
						return false;
					}
					
					build_metalua(chunk_start, scan - chunk_start);

					mp.append('"');

					b8 quit = at(')');
					advance();
					chunk_start = scan;

					if (quit)
						break;

					mp.append(',');
				}

				mp.append(")\n"_str);
			}

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

	if (lua_pcall(L, 0, 1, 0))
	{
		ERROR("failed to run metaenvironment:\n", lua_tostring(L, -1), "\n");
		return false;
	}

	lua_pushstring(L, "__LPP");
	lua_gettable(L, -2);
	lua_pushstring(L, "context");
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

	TRACE(lua_tostring(L, -1), "\n");

	return true;
}
