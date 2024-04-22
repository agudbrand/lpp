#include "uri.h"
#include "assert.h"

#include "logger.h"

#include "ctype.h"

Logger logger = Logger::create("uri"_str, Logger::Verbosity::Warn);

b8 URI::init()
{
	scheme    = dstr::create();
	authority = dstr::create();
	body      = dstr::create();
	return true;
}

void URI::deinit()
{
	scheme.destroy();
	authority.destroy();
	body.destroy();
}

void URI::reset()
{
	scheme.reset();
	authority.reset();
	body.reset();
}

b8 URI::parse(URI* uri, str s_)
{
	str s = s_;

	uri->reset();

	str::pos p = s.find_first(':');
	if (!p)
	{
		ERROR("URI::parse(): failed to find ':' to delimit scheme in given string '", s_, "'\n");
		return false;
	}

	uri->scheme.append(str{s.bytes, p});

	s.bytes = s.bytes + p.x + 1;

	if (s.starts_with("//"_str))
	{
		s.bytes += 2;
		if ((p = s.find_first('/')))
		{
			uri->authority.append(str{s.bytes, p});
			s.bytes = s.bytes + p.x + 1;
		}
		else
		{
			ERROR("URI::parse(): failed to find '/' to delimit authority in given string '", s_, "'\n");
			return false;
		}
	}

	uri->body.append(s);
	return true;
}


