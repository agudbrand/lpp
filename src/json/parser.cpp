#include "parser.h"
#include "logger.h"

#include "stdlib.h"

namespace json
{

/* ------------------------------------------------------------------------------------------------ json::Parser::init
 */
b8 Parser::init(str stream, JSON* json_, str stream_name)
{
	if (!lexer.init(stream, stream_name))
		return false;

	if (!json_)
		return error_no_location("Parser::init() passed a null JSON object. Stream name given is '", stream_name, "'");

	json = json_;

	if (!json->init())
		return false;

	value_stack = ValueStack::create();

	return true;
}

/* ------------------------------------------------------------------------------------------------ json::Parser::deinit
 */
void Parser::deinit()
{
	lexer.deinit();
	
}

/* ------------------------------------------------------------------------------------------------ json::Parser::at
 */
b8 Parser::at(TKind kind)
{
	return curt.kind == kind;
}

/* ------------------------------------------------------------------------------------------------ json::Parser::next_token
 */
void Parser::next_token()
{
	curt = lexer.next_token();
}

/* ------------------------------------------------------------------------------------------------ json::Parser::start
 */
b8 Parser::start()
{
	using enum Token::Kind;

	next_token();
	if (at(Eof))
		return true;

	if (!value())
		return false;

	json->root = value_stack.pop();

	if (at(Eof))
		return true;
	else
		return error_here("more than one value given in JSON text");
}

/* ------------------------------------------------------------------------------------------------ json::Parser::value
 *  Note that this leaves the resulting value at the top of the stack so that the calling function
 *  may use it if needed. The caller must remove it manually.
 */
b8 Parser::value()
{
	switch (curt.kind)
	{
		#define literal_case(k) \
			case TKind::k: \
				if (Value* v = json->new_value(VKind::k)) \
				{ \
					value_stack.push(v); \
					next_token(); \
					return true; \
				} \
				else \
					return error_here("failed to create value for " STRINGIZE(k) " token");
		
		literal_case(Null);
		literal_case(False);
		literal_case(True);

		#undef literal_case

		case TKind::Number:
			if (Value* v = json->new_value(VKind::Number))
			{
				// TODO(sushi) handle this more gracefully later
				//             like just write our own impl of strtod that takes length
				u8 terminated[20];
				if (!curt.raw.null_terminate(terminated, 24))
					return error_here("number value was too large to null-terminate, we only allow numbers up to 24 characters in length at the moment");

				v->number = strtod((char*)terminated, nullptr);

				value_stack.push(v);
				next_token();
				return true;
			}
			else
				return error_here("failed to create value for number token");

		case TKind::String:
			if (Value* v = json->new_value(VKind::String))
			{
				v->string = curt.raw;
				
				value_stack.push(v);
				next_token();
				return true;
			}
			else
				return error_here("failed to create value for string token");

		case TKind::LeftBrace:
			if (Value* v = json->new_value(VKind::Object))
			{
				value_stack.push(v);
				next_token();

				if (!object())
					return false;

				return true;
			}
			else
				return error_here("failed to create value for object");

		case TKind::LeftSquare:
			if (Value* v = json->new_value(VKind::Array))
			{
				value_stack.push(v);
				next_token();
				
				if (!array())
					return false;

				return true;
			}
			else
				return error_here("failed to create value for array");
	
		default: 
			return error_here("expected a value");
	}
}

/* ------------------------------------------------------------------------------------------------ json::Parser::object
 */
b8 Parser::object()
{
	Value*  v = value_stack.head->data;
	Object* o = &v->object;

	if (!o->init())
		return error_here("failed to initialize json::Object");

	if (at(TKind::RightBrace))
	{
		next_token();
		return true;
	}

	for (;;)
	{
		if (!at(TKind::String))
			return error_here("expected a '}' or string for object member name (remember that trailing commas are not allowed in JSON!)");

		str member_name = curt.raw;

		next_token();
		if (!at(TKind::Colon))
			return error_here("expected a ':' to separate member name and value");

		next_token();
		if (!value())
			return false;

		Value* member_value = value_stack.head->data;
		value_stack.pop();

		if (!o->add_member(member_name, member_value))
			return error_here("failed to add member '", member_name, "' to object");

		if (at(TKind::RightBrace))
			break;
	
		if (!at(TKind::Comma))
			return error_here("expected a '}' to end object or ',' to start a new member");

		next_token();
	}

	next_token();
	return true;
}

/* ------------------------------------------------------------------------------------------------ json::Parser::array
 */
b8 Parser::array()
{
	Value* v = value_stack.head->data;
	Array* a = &v->array;

	if (!a->init())
		return error_here("failed to initialize json::Array");

	if (at(TKind::RightSquare))
	{
		next_token();
		return true;
	}

	for (;;)
	{
		if (!value())
			return false;

		Value* array_value = value_stack.pop();
		
		a->push(array_value);
		
		if (at(TKind::RightSquare))
			break;

		if (!at(TKind::Comma))
			return error_here("expected a ']' to end array or ',' delimit new array member");

		next_token();
	}

	next_token();
	return true;
}

} // namespace json
