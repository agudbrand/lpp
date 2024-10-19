#include "uri.h"

#include "iro/logger.h"

#include "assert.h"
#include "ctype.h"

Logger logger = Logger::create("uri"_str, Logger::Verbosity::Warn);

b8 URI::init()
{
    if (!(scheme.open() &&
          authority.open() &&
          body.open()))
        return false;
  return true;
}

void URI::deinit()
{
  scheme.close();
  authority.close();
  body.close();
}

void URI::reset()
{
  scheme.clear();
  authority.clear();
  body.clear();
}

b8 URI::parse(URI* uri, String s_)
{
  String s = s_;

  uri->reset();

  String::pos p = s.findFirst(':');
  if (!p)
  {
    ERROR("URI::parse(): failed to find ':' to delimit scheme in given string '", s_, "'\n");
    return false;
  }

  uri->scheme.write({s.ptr, p.x});

  s.ptr = s.ptr + p.x + 1;

  if (s.startsWith("//"_str))
  {
    s.ptr += 2;
    if ((p = s.findFirst('/')))
    {
      uri->authority.write(String{s.ptr, p.x});
      s.ptr = s.ptr + p.x + 1;
    }
    else
    {
      ERROR("URI::parse(): failed to find '/' to delimit authority in given string '", s_, "'\n");
      return false;
    }
  }

  uri->body.write(s);
  return true;
}


