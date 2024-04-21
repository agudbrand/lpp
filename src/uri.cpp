#include "uri.h"
#include "assert.h"

#include "logger.h"

#include "ctype.h"

namespace uri
{


/* ------------------------------------------------------------------------------------------------ uri::Parser::init
 */
b8 Parser::init(URI* uri_, str s_)
{
	assert(uri_);

	uri = uri_;
	string = s_;
	cursor = s_;

	return true;
}
 
/* ------------------------------------------------------------------------------------------------ uri::Parser::deinit
 */
void Parser::deinit()
{
	uri = nullptr;
	string = cursor = {};
}

/* ------------------------------------------------------------------------------------------------ uri::Parser::start
 */
b8 Parser::start()
{
	assert(uri && string.bytes && cursor.bytes);
}

} // namespace uri

