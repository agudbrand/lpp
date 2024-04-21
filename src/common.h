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

consteval u64 static_string_hash(const char* s)
{
	u64 n = consteval_strlen(s);
	u64 seed = 14695981039;
	while (n--)
	{
		seed ^= (u8)*s;
		seed *= 1099511628211; //64bit FNV_prime
		s++;
	}
	return seed;
}

consteval u64 operator ""_hashed (const char* s)
{
	return static_string_hash(s);
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
 

/* ----------------------------------------------
 *  Printing utils
 */

void print(const char* s);
void print(const u8* s); 
void print(u32 x); 
void print(u64 x);
void print(s32 x);
void print(s64 x);
void print(f64 x);
void print(char c);
void print(void* p);

// help templated stuff give better errors
template<typename T>
concept Printable = requires(T x) 
{
	print(x);
};

template<Printable... T>
void printv(T... args)
{
	(print(args), ...);
}

#ifndef defer
struct defer_dummy {};
template <class F> struct deferrer { F f; ~deferrer() { f(); } };
template <class F> deferrer<F> operator*(defer_dummy, F f) { return {f}; }
#  define DEFER_(LINE) zz_defer##LINE
#  define DEFER(LINE) DEFER_(LINE)
#  define defer auto DEFER(__LINE__) = defer_dummy{} *[&]()
#endif //#ifndef defer

#define STRINGIZE_(a) #a
#define STRINGIZE(a) STRINGIZE_(a)
#define GLUE_(a,b) a##b
#define GLUE(a,b) GLUE_(a,b)

#endif // _lpp_common_h
