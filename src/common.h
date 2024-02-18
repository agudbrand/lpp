#ifndef _lpp_common_h
#define _lpp_common_h

#include "stdint.h"

/* ----------------------------------------------
 *	TODO(sushi) add more log levels and make them
 *	            dependent on defines.
 */
#define TRACE(fmt, ...)                        \
	do {                                       \
		fprintf(stdout, "\e[36mtrace\e[0m: "); \
		fprintf(stdout, fmt, ##__VA_ARGS__);   \
	} while(0); 

/* ----------------------------------------------
 *	Nice typedefs
 */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;
typedef u8       b8; // booean type

/* ----------------------------------------------
 *	Counted string type
 */
typedef struct str
{
	u8* s;
	s32 len;
} str;

#define strl(x) { (u8*)u8##x, sizeof(u8##x)-1 }

/* ----------------------------------------------
 *	str utils
 */
b8 isempty(str s);

/* ----------------------------------------------
 *	Memory wrappers in case I ever want to alter
 *  this behavior later or add memory tracking.
 */
void* memory_reallocate(void* ptr, u64 n_bytes);
void memory_free(void* ptr);
void memory_copy(void* dst, void* src, s32 bytes);

/* ----------------------------------------------
 *	Basic dynamic string type
 */
typedef struct dstr
{
	u8* s;
	s32 len;
	s32 space;
} dstr;

dstr dstr_create(const char* s);
void dstr_destroy(dstr* s);

void dstr_push_cstr(dstr* x, const char* str);
void dstr_push_str(dstr* x, str s);
// pushs the STRING reprentation of 
// 'c' to the dstr, not the character c
void dstr_push_u8(dstr* x, u8 c);
void dstr_push_char(dstr* x, u8 c);

#endif // _lpp_common_h
