/*
 *  Array util
 *  
 *  Kept simple, doesn't consider silly C++ stuff like constructing and such
 *
 *  BUT IT MIGHT have to idk 
 *
 *  This array stores length and space as a header of the actual data.
 */

#ifndef _lpp_array_h
#define _lpp_array_h

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

	T& operator [](s64 i) { return arr[i]; }

	T* begin() { return arr; }
	T* end()   { return arr + len(); }

	void grow_if_needed(s32 new_elements);
};

template<typename T>
void Array<T>::grow_if_needed(s32 new_elements)
{
	Header* header = get_header();

	if (header->len + new_elements <= header->space)
		return;

	while (header->len + new_elements > header->space)
		header->space *= 2;

	header = (Header*)mem.reallocate(header, sizeof(Header) + header->space * sizeof(T));
	arr = (T*)(header + 1);
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
void Array<T>::destroy()
{
	mem.free(get_header());
	arr = nullptr;
}

template<typename T>
T* Array<T>::push()
{
	grow_if_needed(1);

	len() += 1;
	return &arr[len()-1];
}

template<typename T>
void Array<T>::push(const T& x)
{
	grow_if_needed(1);

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
	grow_if_needed(1);

	if (!len()) 
		push(x);
	else
	{
		mem.move(arr + idx + 1, arr + idx, sizeof(T) * (len() - idx));
		len() += 1;
		arr[idx] = x;
	}
}

#endif
