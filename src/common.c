#include "common.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "ctype.h"

b8 isempty(str s)
{
	while (s.len--) if (!isspace(s.s[s.len])) return 0;
	return 1;
}

u64 hash_string(str s)
{
	u64 seed = 14695981039;
	while (s.len--)
	{
		seed ^= (u8)*s.s;
		seed *= 1099511628211;
		s.s += 1;
	}
	return seed;
}

void* memory_reallocate(void* ptr, u64 n_bytes)
{
	return realloc(ptr, n_bytes);
}

void memory_free(void* ptr)
{
	free(ptr);
}

void memory_copy(void* dst, void* src, s32 bytes)
{
	memcpy(dst, src, bytes);
}

dstr dstr_create(const char* s)
{
	dstr out = {};

	if (s)
	{
		out.len = strlen(s);
		out.s = memory_reallocate(0, out.len);
		memory_copy(out.s, (char*)s, out.len);
	}
	else
	{
		out.space = 4;
		out.len = 0;
		out.s = memory_reallocate(0, out.space);
	}

	return out;
}

void dstr_destroy(dstr* s)
{
	memory_free(s->s);
	s->s = 0;
	s->len = 0;
}

static void dstr_grow_if_needed(dstr* x, s32 bytes)
{
	if (x->len + bytes <= x->space)
		return;

	// TRACE("grow by %i\n", bytes);

	while(x->space < x->len + bytes) x->space *= 2;
	x->s = memory_reallocate(x->s, x->space);
}

void dstr_push_cstr(dstr* x, const char* str)
{
	u32 len = strlen(str);
	dstr_grow_if_needed(x, len);
	memory_copy(x->s + x->len, (void*)str, len);
	x->len += len;
}

void dstr_push_str(dstr* x, str s)
{
	dstr_grow_if_needed(x, s.len);
	memory_copy(x->s + x->len, s.s, s.len);
	x->len += s.len;
}

// pushs the STRING reprentation of 
// 'c' to the dstr, not the character c
void dstr_push_u8(dstr* x, u8 c)
{
	char buf[3];
	s32 n = snprintf(buf, 3, "%u", c);
	dstr_grow_if_needed(x, n);
	memory_copy(x->s + x->len, buf, n);
	x->len += n;
}

void dstr_push_s64(dstr* x, s64 c)
{
	char buf[21];
	s32 n = snprintf(buf, 21, "%li", c);
	dstr_grow_if_needed(x, n);
	memory_copy(x->s + x->len, buf, n);
	x->len += n;
}

void dstr_push_char(dstr* x, u8 c)
{
	dstr_grow_if_needed(x, 1);
	x->s[x->len] = c;
	x->len += 1;
}

