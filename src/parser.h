/*
 *  Parser for lpp files. Takes tokens and transforms them into a metaprogram.
 */

#ifndef _lpp_parser_h
#define _lpp_parser_h

#include "common.h"
#include "lex.h"
#include "csetjmp"

struct Parser
{
	Logger logger;

	Array<Token> tokens;

	Lexer lexer;

	Token* curt;

	Source* source;

	io::IO* in;
	io::IO* out;

	jmp_buf err_handler;

	b8   init(Source* source, io::IO* instream, io::IO* outstream, Logger::Verbosity verbosity = Logger::Verbosity::Warn);
	void deinit();

	b8 run();

private:

	b8 nextToken();

	b8 at(Token::Kind kind);

	str getRaw();

	template<typename... T>
	void writeOut(T... args)
	{
		io::formatv(out, args...);
	}

};

#endif // _lpp_parser_h
