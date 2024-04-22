#include "types.h"

namespace json
{

/* ------------------------------------------------------------------------------------------------ json::Array::init
 */
b8 Array::init()
{
	values = ValueArray::create(4);
	return true;
}

/* ------------------------------------------------------------------------------------------------ json::Array::deinit
 */
void Array::deinit()
{
	values.destroy();
}

/* ------------------------------------------------------------------------------------------------ json::Array::push
 */
void Array::push(Value* v)
{
	values.push(v);
}

/* ------------------------------------------------------------------------------------------------ json::Array::pop
 */
Value* Array::pop()
{
	Value* v = *(values.arr + values.len() - 1);
	values.pop();
	return v;
}

/* ------------------------------------------------------------------------------------------------ json::Object::init
 */
b8 Object::init()
{
	members = MemberMap::create();
	pool = MemberPool::create();

	return true;
}

/* ------------------------------------------------------------------------------------------------ json::Object::deinit
 */
void Object::deinit()
{
	members.destroy();
	pool.destroy();
}

/* ------------------------------------------------------------------------------------------------ json::Object::add_member
 */
b8 Object::add_member(str name, Value* value)
{
	Member* m = pool.add();
	if (!m)
		return false;

	m->name = name;
	m->hash = name.hash();
	m->value = value;

	members.insert(m);

	return true;
}

/* ------------------------------------------------------------------------------------------------ json::Object::find_member
 */
Value* Object::find_member(str name)
{
	if (Member* member = members.find(name.hash()))
		return member->value;
	return nullptr;
}


/* ------------------------------------------------------------------------------------------------ json::JSON::init
 */
b8 JSON::init()
{
	pool = ValuePool::create();
	root = nullptr;
	return true;
}

/* ------------------------------------------------------------------------------------------------ json::JSON::deinit
 */
void JSON::deinit()
{
	pool.destroy();
	root = nullptr;
}

/* ------------------------------------------------------------------------------------------------ json::JSON::new_value
 */
Value* JSON::new_value(Value::Kind kind)
{
	Value* v = pool.add();
	if (!v)
		return nullptr;
	v->kind = kind;
	return v;
}

/* ------------------------------------------------------------------------------------------------ pretty_print_recur
 */
void pretty_print_recur(io::IO* out, Value* value, s32 depth)
{
	using enum Value::Kind;

	switch (value->kind)
	{
		case Null: 
			out->write("null"_str);
			return;
		case False:
			out->write("false"_str);
			return;
		case True:
			out->write("true"_str);
			return;
		case Number:
            io::format(out, value->number);
			return;
		case String:
            io::formatv(out, "\"", value->string, "\"");
			return;
		case Array:
			out->write("[\n"_str);
			for (s32 i = 0, len = value->array.values.len(); i < len; i++)
			{
				for (s32 i = 0; i < depth + 1; i++)
					out->write(" "_str);
				pretty_print_recur(out, value->array.values[i], depth + 2);
				if (i != len - 1)
					out->write(","_str);
				out->write("\n"_str);
			}
			for (s32 i = 0; i < depth-1; i++)
				out->write(" "_str);
			out->write("]"_str);
			return;
		case Object:
			out->write("{\n"_str);
			for (auto& member : value->object.members)
			{
				for (s32 i = 0; i < depth + 1; i++)
					out->write(" "_str);
                io::formatv(out, "\"", member.name, "\": ");
				pretty_print_recur(out, member.value, depth + 2);
				out->write("\n"_str);
			}
			for (s32 i = 0; i < depth-1; i++)
				out->write(" "_str);
			out->write("}"_str);
			return;
	}
}

/* ------------------------------------------------------------------------------------------------ json::JSON::pretty_print
 */
s64 JSON::pretty_print(io::IO* out)
{
	pretty_print_recur(out, root, 0);
    return 0; // TODO(sushi) return number written later... maybe
}

} // namespace json
