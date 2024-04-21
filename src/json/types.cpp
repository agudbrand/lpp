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
void pretty_print_recur(Value* value, s32 depth)
{
	using enum Value::Kind;

	switch (value->kind)
	{
		case Null: 
			print("null");
			return;
		case False:
			print("false");
			return;
		case True:
			print("true");
			return;
		case Number:
			print(value->number);
			return;
		case String:
			printv("\"", value->string, "\"");
			return;
		case Array:
			print("[\n");
			for (s32 i = 0, len = value->array.values.len(); i < len; i++)
			{
				for (s32 i = 0; i < depth + 1; i++)
					print(" ");
				pretty_print_recur(value->array.values[i], depth + 2);
				if (i != len - 1)
					print(",");
				print("\n");
			}
			for (s32 i = 0; i < depth; i++)
				print(" ");
			print("]");
			return;
		case Object:
			print("{\n");
			for (auto& member : value->object.members)
			{
				for (s32 i = 0; i < depth + 1; i++)
					print(" ");
				printv("\"", member.name, "\": ");
				pretty_print_recur(member.value, depth + 2);
				print("\n");
			}
			for (s32 i = 0; i < depth; i++)
				print(" ");
			print("}");
			return;
	}
}

/* ------------------------------------------------------------------------------------------------ json::JSON::pretty_print
 */
void JSON::pretty_print()
{
	pretty_print_recur(root, 0);
}

} // namespace json
