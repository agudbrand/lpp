/* Common Lua 
 *	
 *	 A program for running lua scripts that may need access to the
 *	 common functionality used throughout lpp. common.o is linked
 *	 into this program so any function defined there may be used 
 *	 by lua scripts.
 *     
 */

extern "C" 
{

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

}

#include "common.h"

int main(int argc, char** argv)
{
	lua_State* L = lua_open();
	luaL_openlibs(L);

	if (luaL_loadfile(L, argv[1]) || lua_pcall(L, 0, 0, 0))
	{
		fprintf(stdout, "%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return 1;
	}

	return 0;
}
