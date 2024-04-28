#ifndef _lpp_common_h
#define _lpp_common_h

#include "stdint.h"
#include "stddef.h"

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

consteval s64 consteval_strlen(const char* s) {
    s64 i = 0;
    while(s[i])
        i++;
    return i;
}

consteval u64 static_string_hash(const char* s, size_t len)
{
	u64 seed = 14695981039;
	for (s32 i = 0; i < len; i++)
	{
		seed ^= (u8)s[i];
		seed *= 1099511628211;
	}
	return seed;
}

consteval u64 operator ""_hashed (const char* s, size_t len)
{
	return static_string_hash(s, len);
}

/* ----------------------------------------------
 *	Memory wrappers in case I ever want to alter
 *  this behavior later or add memory tracking.
 */

struct Mem
{
	void* allocate(u64 n_bytes);
	void* reallocate(void* ptr, u64 n_bytes);
	void  free(void* ptr);
	void  copy(void* dst, void* src, u64 bytes);
	void  move(void* dst, void* src, u64 bytes);
};

extern Mem mem; 
 
#ifndef defer
struct defer_dummy {};
struct defer_with_cancel_dummy {};
template <class F> struct deferrer { F f; ~deferrer() { f(); } };
template<typename F> struct deferrer_with_cancel { b8 canceled; F f; ~deferrer_with_cancel() { if (!canceled) f(); }  void cancel() { canceled = true; } };
template <class F> deferrer<F> operator*(defer_dummy, F f) { return {f}; }
template <class F> deferrer_with_cancel<F> operator*(defer_with_cancel_dummy, F f) { return {false, f}; }
#  define DEFER_(LINE) zz_defer##LINE
#  define DEFER(LINE) DEFER_(LINE)
#  define defer auto DEFER(__LINE__) = defer_dummy{} *[&]()
#  define defer_with_cancel defer_with_cancel_dummy{} *[&]()
#endif //#ifndef defer

#define STRINGIZE_(a) #a
#define STRINGIZE(a) STRINGIZE_(a)
#define GLUE_(a,b) a##b
#define GLUE(a,b) GLUE_(a,b)

#endif // _lpp_common_h
