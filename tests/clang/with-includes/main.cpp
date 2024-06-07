#include "vec.h"

$$$
require("lppclang").init(lpp, "../lppclang/build/debug/liblppclang.so")

local printFields = function()
	local section = lpp.getSectionAfterMacro()

	local fulltext = lpp.getOutputSoFar()..section:getString()

	local ast = lpp.clang.parseString(fulltext, 
			{
				include_dirs = { "tests/clang/with-includes" }
			})
	local tuiter = ast:getTranslationUnitDecl():getDeclIter()

	local d = tuiter:next()

	print(d:name())

	local members = d:getDeclIter()

	while true do
		local member = members:next()

		if not member then
			break
		end

		if not member:isField() then
			goto continue
		end

		io.write(member:name(), ": ", member:type():getTypeName(), "\n")

		::continue::
	end

end

$$$

@printFields
struct Player
{
	vec3 pos;
};
