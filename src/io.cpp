#include "io.h"

#include "stdlib.h"
#include "stdio.h"

// TODO(sushi) platform layers
#include "fcntl.h"
#include "sys/stat.h"
#include "unistd.h"

#include "dirent.h"
#include "sys/stat.h"
#include "errno.h"
#include "glob.h"

#include "assert.h"

namespace io
{

s64 format(IO* io, u8 x)
{
	u8  buffer[32];
	s32 len = snprintf((char*)buffer, 32, "%hhu", x);
	return io->write({buffer, len});
}

s64 format(IO* io, u16 x)
{
	u8  buffer[32];
	s32 len = snprintf((char*)buffer, 32, "%hu", x);
	return io->write({buffer, len});
}

s64 format(IO* io, u32 x)
{
	u8  buffer[32];
	s32 len = snprintf((char*)buffer, 32, "%u", x);
	return io->write({buffer, len});
}

s64 format(IO* io, u64 x)
{
	u8  buffer[32];
	s32 len = snprintf((char*)buffer, 32, "%lu", x);
	return io->write({buffer, len});
}

s64 format(IO* io, s8 x)
{
	u8  buffer[32];
	s32 len = snprintf((char*)buffer, 32, "%hhi", x);
	return io->write({buffer, len});
}

s64 format(IO* io, s16 x)
{
	u8  buffer[32];
	s32 len = snprintf((char*)buffer, 32, "%hi", x);
	return io->write({buffer, len});
}

s64 format(IO* io, s32 x)
{
	u8  buffer[32];
	s32 len = snprintf((char*)buffer, 32, "%i", x);
	return io->write({buffer, len});
}

s64 format(IO* io, s64 x)
{
	u8  buffer[32];
	s32 len = snprintf((char*)buffer, 32, "%li", x);
	return io->write({buffer, len});
}

s64 format(IO* io, f32 x)
{
	u8  buffer[32];
	s32 len = snprintf((char*)buffer, 32, "%f", x);
	return io->write({buffer, len});
}

s64 format(IO* io, f64 x)
{
	u8  buffer[32];
	s32 len = snprintf((char*)buffer, 32, "%f", x);
	return io->write({buffer, len});
}

s64 format(IO* io, str x)
{
	return io->write(x);
}

s64 format(IO* io, dstr x)
{
	return io->write(x.fin);
}

s64 format(IO* io, char x)
{
	return io->write({(u8*)&x, 1});
}

s64 format(IO* io, void* x)
{
	u8 buffer[32];
	s32 len = snprintf((char*)buffer, 32, "%p", x);
	return io->write({buffer, len});
}

s64 format(IO* io, const char* x)
{
	s64 n = 0;
	while (x[n])
	{
		if (!format(io, x[n]))
			break;
		n += 1;
	}
	return n;
}

s64 format(IO* io, const fmt::SanitizeControlCharacters& x)
{
	s64 bytes_written = 0;
	str s = x.x;
	while (!s.isempty())
	{
		utf8::Codepoint c = s.advance();

		switch (c.codepoint)
		{
			case '\a': bytes_written += io->write("\\a"_str); break;
			case '\b': bytes_written += io->write("\\b"_str); break;
			case '\e': bytes_written += io->write("\\e"_str); break;
			case '\f': bytes_written += io->write("\\f"_str); break;
			case '\n': bytes_written += io->write("\\n"_str); break;
			case '\r': bytes_written += io->write("\\r"_str); break;
			case '\t': bytes_written += io->write("\\t"_str); break;
			case '\v': bytes_written += io->write("\\v"_str); break;
			default: {
				// lol
				utf8::Char ch = utf8::encode_character(c.codepoint);
				bytes_written += io->write({ch.bytes, ch.count});
			} break;
		}
	}
	return bytes_written;
}

void Memory::grow_if_needed(s64 wanted_space)
{
	if (space - len > wanted_space)
		return;

	while (space - len < wanted_space)
		space *= 2;

	buffer = (u8*)mem.reallocate(buffer, space);
}

b8 Memory::open(s32 initial_space)
{
	if (flags.test(Flag::Open))
		return true;

	flags.set(Flag::Writable);
	flags.set(Flag::Readable);

	space = initial_space;
	pos = len = 0;
	buffer = (u8*)mem.allocate(space);

	if (buffer != nullptr)
	{
		flags.set(Flag::Open);
		return true;
	}
	return false;
}


void Memory::close()
{
	if (!flags.test(Flag::Open))
		return;

	mem.free(buffer);
	buffer = nullptr;
	len = space = 0;
	pos = 0;

	flags.unset(Flag::Open);
}

u8* Memory::reserve(s32 wanted_space)
{
	grow_if_needed(wanted_space);
	
	return buffer + len;
}

void Memory::commit(s32 committed_space)
{
	assert(len + committed_space <= space);
	len += committed_space;
	buffer[len] = 0;
}

s64 Memory::write(str slice)
{
	grow_if_needed(slice.len);

	mem.copy(buffer + len, slice.bytes, slice.len);
	len += slice.len;

	buffer[len] = 0;

	return slice.len;
}

s64 Memory::read(str outbuffer)
{
	if (pos == len)
		return 0;

	s64 bytes_remaining = len - pos;
	s64 bytes_to_read = (bytes_remaining > outbuffer.len ? outbuffer.len : bytes_remaining);
	mem.copy(outbuffer.bytes, buffer + pos, bytes_to_read);
	pos += bytes_to_read;
	return bytes_to_read;
}

s64 Memory::read_from(s64 pos, str slice)
{
	assert(pos < len);

	s64 bytes_remaining = len - pos;
	s64 bytes_to_read = (bytes_remaining > slice.len ? slice.len : bytes_remaining);
	mem.copy(slice.bytes, buffer + pos, bytes_to_read);
	return bytes_to_read;
}

Memory::Rollback Memory::create_rollback()
{
	return len;
}

void Memory::commit_rollback(Rollback rollback)
{
	len = rollback;
}

b8 FileDescriptor::open(u64 fd_)
{
	if (is_open())
		return true;

	fd = fd_;

	// TODO(sushi) set these based on if we're given stdin/out/err
	set_writable();
	set_readable();
	set_open();

	return true;
}

b8 FileDescriptor::open(str path, io::Flags io_flags, Flags fd_flags)
{
	using enum io::Flag;
	if (is_open())
		return true;

	if (!io_flags.test_any<Writable, Readable>())
		return false;

	static const u32 max_path_size = 255;
	u8 ntbuf[max_path_size];
	if (!path.null_terminate(ntbuf, max_path_size))
		return false;

	int flags = 0;

	if (io_flags.test_all<Writable, Readable>())
	{
		flags |= O_RDWR;
		set_writable();
		set_readable();
	}
	else if (io_flags.test(Readable))
	{
		flags |= O_RDONLY;
		set_readable();
	}
	else if (io_flags.test(Writable))
	{
		flags |= O_WRONLY;
		set_writable();
	}

	if (fd_flags.test(Flag::NonBlocking))
		flags |= O_NONBLOCK;

	if (fd_flags.test(Flag::Create))
		flags |= O_CREAT;

	fd = ::open((char*)ntbuf, flags, S_IRUSR | S_IWUSR);

	if (fd == -1)
	{
		perror("FileDescriptor::open() in call to ::open()");
		return false;
	}

	set_open();
	return true;
}

void FileDescriptor::close()
{
	using enum io::Flag;

	if (!is_open() || flags.test(Flag::View))
		return;

	if (::close(fd) == -1)
	{
		perror("FileDescriptor::close() in call to ::close()");
	}

	unset_open();
}

s64 FileDescriptor::write(str slice)
{
	if (!is_open() || !can_write())
		return 0;

	int r = ::write(fd, slice.bytes, slice.len);

	if (r == -1)
	{
		perror("FileDescriptor::write() in call to ::write()");
		return 0;
	}
	return r;
}

s64 FileDescriptor::read(str slice)
{
	if (!is_open() || !can_read())
		return 0;

	int r = ::read(fd, slice.bytes, slice.len);

	if (r == -1)
	{
		perror("FileDescriptor::read() in call to ::read()");
		return 0;
	}

	return r;
}


template<size_t N>
void StaticBuffer<N>::clear()
{
	write_pos = read_pos = 0;
}

template<size_t N>
void StaticBuffer<N>::rewind()
{
	read_pos = 0;
}

template<size_t N>
s64 StaticBuffer<N>::write(str slice)
{
	if (write_pos == len)
		return 0;

	s64 space_remaining = len - write_pos;
	s64 bytes_to_write = (slice.len > space_remaining ? space_remaining : slice.len);
	mem.copy(buffer + write_pos, slice.bytes, bytes_to_write);
	write_pos += bytes_to_write;
	buffer[write_pos] = 0;
	return bytes_to_write;
}

template<size_t N>
s64 StaticBuffer<N>::read(str slice)
{
	if (read_pos == write_pos)
		return 0;

	s64 space_remaining = write_pos - read_pos;
	s64 bytes_to_read = (slice.len < space_remaining ? slice.len : space_remaining);
	mem.copy(slice.bytes, buffer + read_pos, bytes_to_read);
	read_pos += bytes_to_read;
	return bytes_to_read;
}

} // namespace io
