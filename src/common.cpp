#include "common.h"

#include "stdlib.h"
#include "ctype.h"
#include "string.h"
#include "stdio.h"

b8 str::isempty()
{
	s32 x = len;
	while (x--) if (!isspace(s[x])) return 0;
	return 1;
}

u64 str::hash()
{
	u8* p = s;
	s32 x = len;
	u64 seed = 14695981039;
	while (x--)
	{
		seed ^= (u8)*p;
		seed *= 1099511628211;
		p += 1;
	}
	return seed;
}

b8 str::equal(const char* s)
{
	return 0 == strncmp((char*)this->s, s, len);
}

u8* str::null_terminate(u8* buffer, s32 buffer_size)
{
	if (buffer_size <= len)
	{
		u8* dyn = (u8*)mem.allocate(len+1);
		mem.copy(dyn, s, len);
		dyn[len] = 0;
		return dyn;
	}

	mem.copy(buffer, s, len);
	buffer[len] = 0;
	return buffer;
}

str operator ""_str (const char* s, size_t length)
{
	return {(u8*)s, (s32)length};
}

Mem mem;

void* Mem::allocate(u64 n_bytes)
{
	return malloc(n_bytes);
}

void Mem::free(void* ptr)
{
	::free(ptr);
}

void* Mem::reallocate(void* ptr, u64 n_bytes)
{
	return realloc(ptr, n_bytes);
}

void Mem::copy(void* dst, void* src, u64 n_bytes)
{
	memcpy(dst,src,n_bytes);
}

void Mem::move(void* dst, void* src, u64 n_bytes)
{
	memmove(dst, src, n_bytes);
}

void print(const char* s) { printf("%s", s); }
void print(const u8* s) { printf("%s", s); }

void print(str s)  { printf("%.*s", s.len, s.s); }
void print(dstr s) { printf("%.*s", s.len, s.s); }
void print(u32 x)  { printf("%u", x); }
void print(u64 x)  { printf("%lu", x); }
void print(s32 x)  { printf("%i", x); }
void print(s64 x)  { printf("%li", x); }

void print(char x) { printf("%c", x); }

void print(void* x) { printf("%p", x); }

dstr dstr::create(const char* s)
{
	dstr out = {};
	if (s)
	{
		out.len = strlen(s);
		out.space = out.len * 2;
		out.s = (u8*)mem.allocate(sizeof(u8) * out.space);
		mem.copy(out.s, (void*)s, out.len);
	}
	else
	{
		out.len = 0;
		out.space = 8;
		out.s = (u8*)mem.allocate(sizeof(u8) * out.space);
	}

	return out;
}

void dstr::destroy()
{
	mem.free(s);
	len = space = 0;
}	

void grow_if_needed(dstr* x, s32 new_elems)
{
	if (x->len + new_elems <= x->space)
		return;

	while (x->space < x->len + new_elems)
		x->space *= 2;

	x->s = (u8*)mem.reallocate(x->s, x->space);
}

void dstr::append(const char* x)
{
	s32 xlen = strlen(x);

	grow_if_needed(this, xlen);

	mem.copy(s+len, (void*)x, xlen);

	len += xlen;
}

void dstr::append(str x)
{
	grow_if_needed(this, x.len);
	
	mem.copy(s+len, (void*)x.s, x.len);

	len += x.len;
}

void dstr::append(s64 x)
{
	grow_if_needed(this, 22);
	len += snprintf((char*)(s + len), 22, "%li", x);
}


