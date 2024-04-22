/*
 *  IO api
 */

#ifndef _lpp_io_h
#define _lpp_io_h

#include "common.h"
#include "unicode.h"

#include "flags.h"
#include "assert.h"

namespace io
{

struct IO;

s64 format(IO* io, u8  x);
s64 format(IO* io, u16 x);
s64 format(IO* io, u32 x);
s64 format(IO* io, u64 x);
s64 format(IO* io, s8  x);
s64 format(IO* io, s16 x);
s64 format(IO* io, s32 x);
s64 format(IO* io, s64 x);
s64 format(IO* io, f32 x);
s64 format(IO* io, f64 x);
s64 format(IO* io, b8  x);
s64 format(IO* io, str x);
s64 format(IO* io, dstr x);
s64 format(IO* io, char x);
s64 format(IO* io, void* x);
s64 format(IO* io, const char* x);

template<typename T>
concept Formattable = requires(IO* io, T x)
{
	format(io, x);
};

template<Formattable... T>
s64 formatv(IO* io, T... args)
{
	s64 accumulator = 0;
	((accumulator += format(io, args)), ...);
	return accumulator;
}

enum class Flag
{
	Open,
	Writable,
	Readable,
};

typedef ::Flags<Flag> Flags; 

/* ================================================================================================ io::IO
 *  Base interface for input/output
 */
struct IO
{
	Flags flags = {};

	// Writes the contents of 'slice' into this IO and returns the number of bytes written.
	virtual s64 write(str slice) = 0;

	// Reads the contents of this IO into the buffer at 'buffer.bytes' up to 'buffer.len' and 
	// returns the number of bytes read.
	virtual s64 read(str buffer) = 0;

	virtual b8   open()  { flags.set(Flag::Open);   return true; }
	virtual void close() { flags.unset(Flag::Open); }
	virtual b8   flush() { return true; }

	// implementable on IO that can seek
	// reads into buffer starting at 'pos'
	virtual s64 read_from(s64 pos, str buffer) { assert(false); return 0; }

	b8 is_open()   { return flags.test(Flag::Open); }
	b8 can_read()  { return flags.test(Flag::Readable); }
	b8 can_write() { return flags.test(Flag::Writable); }

protected:

	// helpers for use internally, especially for things that define their own 'flags' like FileDescriptor
	void set_open()     { flags.set(Flag::Open); }
	void set_writable() { flags.set(Flag::Writable); }
	void set_readable() { flags.set(Flag::Readable); }

	void unset_open()     { flags.unset(Flag::Open); }
	void unset_writable() { flags.unset(Flag::Writable); }
	void unset_readable() { flags.unset(Flag::Readable); }
};

/* ================================================================================================ io::Memory
 *  IO around a memory buffer that may grow.
 *
 *  TODO(sushi) implement allocators and make this take one.
 */
struct Memory : public IO
{
	u8* buffer;
	s64 len;
	s64 space;

	// position in buffer for reading
	s64 pos;

	b8 open() override { return open(8); }
	b8 open(s32 initial_space);

	void close() override;

	// Clears the buffer and resets the read position to 0.
	void clear() { len = pos = 0; }

	// Resets the position being read.
	void rewind() { pos = 0; }

	// Returns if the read position is at the end of the buffer.
	b8 at_end() { return pos == len; }

	// Attempts to reserve 'space' bytes and returns a pointer to 
	// the reserved memory. commit() should be used after a call 
	// to this function to indicate how many bytes were actually
	// used.
	u8* reserve(s32 space);

	// Commits space reserved by reserve().
	void commit(s32 space);

	str as_str() { return {buffer, len}; }

	s64 write(str slice) override;
	s64 read(str slice) override;

	s64 read_from(s64 pos, str slice) override;

private: 
	
	void grow_if_needed(s64 space);

};

/* ================================================================================================ io::FileDescriptor
 *  IO around a system file descriptor.
 *
 *  Currently only supporting linux for now. 
 *  TODO(sushi) split up the implementation of this into platform layers and implement Windows.
 */
struct FileDescriptor : public IO
{
	u64 fd;

	enum class Flag
	{
		View, // the file descriptor is not one that we opened, so we will not allow using 'close' on it
		NonBlocking, // will not block on reads or writes
		Create, // if a file at the given path does not exist it will be created
	};
	typedef ::Flags<Flag> Flags;

	Flags flags;

	[[deprecated("io::FileDescriptor must be given a file descriptor to open!")]]
	b8 open() override { return false; }

	// Creates a 'view' on an already existing file descriptor.
	// All I think I'll use this for atm is stdout/err
	// This just assumes the given fd is read/writable for now
	// TODO(sushi) figure out if we can test for read/write-ability somehow
	b8 open(u64 fd);

	b8 open(
		str path, 
		io::Flags io_flags, 
		Flags fd_flags = Flags::none());

	b8 open(
		str path,
		io::Flag io_flag,
		Flags fd_flags = Flags::none())
	{
		return open(path, io::Flags::from(io_flag), fd_flags);
	}

	b8 open(
		str path,
		io::Flag io_flag,
		Flag fd_flag)
	{
		return open(path, io::Flags::from(io_flag), Flags::from(fd_flag));
	}

	void close() override;

	virtual s64 write(str slice) override;
	virtual s64 read(str slice) override;
};

/* ================================================================================================ io::StaticBuffer
 *  IO around a statically sized buffer.
 *  This ensures that the buffer can always be null-terminated, so it makes room for N+1 bytes 
 *  and on every write sets the byte after the write_pos to 0.
 *
 *  Because this is typically used where I need to something that takes a C string, an implicit 
 *  conversion to C strings is defined on this type.
 */
template<size_t N>
struct StaticBuffer : public IO
{
	static const size_t len = N;

	u8  buffer[N+1];
	s32 write_pos;
	s32 read_pos;

	size_t capacity() { return len; }
	str as_str() { return str{buffer, write_pos}; }

	operator char*() { return (char*)buffer; }

	void clear();
	void rewind();

	virtual s64 write(str slice) override;
	virtual s64 read(str slice) override;
};


};


#endif // _lpp_io_h
