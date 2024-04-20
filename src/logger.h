/*
 *  Logging stuff
 */

#ifndef _lake_logger_h
#define _lake_logger_h

#include "common.h"

#define __HELPER(v, ...) if ((u32)log.verbosity <= (u32)Logger::Verbosity::v) log.log(Logger::Verbosity::v, __VA_ARGS__)

#define TRACE(...) __HELPER(Trace, __VA_ARGS__)
#define DEBUG(...) __HELPER(Debug, __VA_ARGS__)
#define INFO(...) __HELPER(Info, __VA_ARGS__)
#define NOTICE(...) __HELPER(Notice, __VA_ARGS__)
#define WARN(...) __HELPER(Warn, __VA_ARGS__)
#define ERROR(...) __HELPER(Error, __VA_ARGS__)
#define FATAL(...) __HELPER(Fatal, __VA_ARGS__)

#define SCOPED_INDENT ScopedIndent __FILE__##__LINE__

struct Logger
{
	enum class Verbosity : u32
	{
		Trace,
		Debug,
		Info,
		Notice,
		Warn,
		Error,
		Fatal,
	};

	Verbosity verbosity;

	u64 indentation;

	template<typename... T>
		requires (Printable<T> && ...)
	void log(Verbosity v, T... args)
	{
		using enum Verbosity;

		switch (v)  
		{
			case Trace:  print(strl(" trace: ")); break;
			case Debug:  print(strl(" debug: ")); break;
			case Info:   print(strl("  info: ")); break;
			case Notice: print(strl("notice: ")); break;
			case Warn:   print(strl("  warn: ")); break;
			case Error:  print(strl(" error: ")); break;
			case Fatal:  print(strl(" fatal: ")); break;
		}

		for (u32 i = 0; i < indentation; i++)
			print("  ");

		(print(args), ...);
	}
};
  
extern Logger log; 

struct ScopedIndent
{
	ScopedIndent()
	{
		log.indentation += 1;
	}

	~ScopedIndent()
	{
		if (log.indentation)
			log.indentation -= 1;
	}
};

#endif
