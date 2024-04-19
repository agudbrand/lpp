/*
 *  Array util
 *  
 *  Kept simple, doesn't consider silly C++ stuff like constructing and such
 */

#ifndef _lake_array_h
#define _lake_array_h

#include "common.h"

template<typename T>
struct Array 
{
	struct Header
	{
		s32 len;
		s32 space;
	};

	T* arr;

	static Array<T> create(s32 init_space = 8);

	void destroy();

	s32& len();
	s32& space();

	T*   push();
	void push(const T& x);
	void pop();

	void insert(s32 idx, const T& x);

	Header* get_header() { return (Header*)arr - 1; }
};

template<typename T>
void grow_if_needed(Array<T>* a, const s32 new_elements)
{
	typedef typename Array<T>::Header Header;

	if (a->len() + new_elements <= a->space())
		return;

	while (a->len() + new_elements > a->space())
		a->space() *= 2;

	a->arr = (T*)(-1 + (Header*)mem.allocate(sizeof(Header) + a->space() * sizeof(T)));
}

template<typename T>
s32& Array<T>::len() 
{
	return get_header()->len;
}

template<typename T>
s32& Array<T>::space() 
{
	return get_header()->space;
}

template<typename T>
Array<T> Array<T>::create(s32 init_space)
{
	init_space = (8 > init_space ? 8 : init_space);

	Array<T> out = {};

	Header* header = (Header*)mem.allocate(sizeof(Header) + init_space * sizeof(T));

	header->space = init_space;
	header->len = 0;

	out.arr = (T*)(header + 1);

	return out;
}

template<typename T>
T* Array<T>::push()
{
	grow_if_needed(this, 1);

	len() += 1;
	return &arr[len()-1];
}

template<typename T>
void Array<T>::push(const T& x)
{
	grow_if_needed(this, 1);

	arr[len()] = x;
	len() += 1;
}

template<typename T>
void Array<T>::pop()
{
	len() -= 1;
}

template<typename T>
void Array<T>::insert(s32 idx, const T& x)
{
	grow_if_needed(this, 1);

	if (!len()) push(x);
	else
	{
		mem.move(arr + idx + 1, arr + idx, sizeof(T) * (len() - idx));
		len() += 1;
		arr[idx] = x;
	}
}

#endif
