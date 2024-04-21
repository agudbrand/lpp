/* 
 *  Helper for dealing with enum class flags.
 *  
 *  Nothing crazy fancy, just supports up to 64 flags based on a given enum.
 */

#ifndef _lake_flags_h
#define _lake_flags_h

#include "common.h"

template<typename T>
struct Flags
{
	typedef u64 FlagType;

	FlagType flags;
	
	static Flags<T> none() { return {}; }
	static Flags<T> all()  { return {(FlagType)-1}; }

	template<typename... Args>
	static constexpr Flags<T> from(T first, Args... args) 
	{ 
		Flags<T> out = {};
		out.set(first);
		(out.set(args), ...);
		return out;
	}

	b8 test_all(T x)
	{
		return (flags & (FlagType)x) == (FlagType)x;
	}

	b8 test_any(T x)
	{
		return (flags & (FlagType)x);
	}

	b8 test(T x)
	{
		return flags & ((FlagType)1 << (FlagType)x);
	}

	void set(T x)
	{
		flags |= ((FlagType)1 << (FlagType)x);
	}
};

#endif
