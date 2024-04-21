/*
 *
 *  Pool structure for making chunks of unmoving memory and allocating from them in an 
 *  efficient manner;
 *
 */

#ifndef _lpp_pool_h
#define _lpp_pool_h

#include "common.h"
#include "stddef.h"

template
<
	typename T, // element type
	s32 N_slots_per_chunk = 16
>
struct Pool 
{
	// really only because the one arg is so long 
	// and ive never done this before so i want to 
	// see how it feels
	typedef Pool<T, N_slots_per_chunk> Self;

	struct Slot
	{
		b8 used;
		union
		{
			T     element;
			Slot* next_free_slot;
		};
	};

	struct Chunk
	{
		Chunk* next;
		Slot   slots[N_slots_per_chunk];
	};
	

	Chunk* current_chunk;
	Slot*  free_slot;


	/* -------------------------------------------------------------------------------------------- 
	 */


	/* -------------------------------------------------------------------------------------------- create
	 */ 
	static Self create()
	{
		Self out = {};
		
		out.new_chunk();
		out.free_slot = out.current_chunk->slots;

		return out;
	}

	/* -------------------------------------------------------------------------------------------- destroy
	 */ 
	void destroy()
	{
		Chunk* next = current_chunk->next;
		for (;current_chunk;)
		{
			mem.free(current_chunk);
			current_chunk = next;
		}

		current_chunk = nullptr;
		free_slot = nullptr;
	}

	/* -------------------------------------------------------------------------------------------- add
	 */ 
	T* add()
	{
		Slot* slot = free_slot;
		if (!slot->next_free_slot)
		{
			new_chunk();
			slot->next_free_slot = current_chunk->slots;
		}

		free_slot = slot->next_free_slot;

		slot->used = true;
		slot->element = {};
		return &slot->element;
	}

	void add(const T& x)
	{
		T* v = add();
		*v = x;
		return v;
	}

	/* -------------------------------------------------------------------------------------------- remove
	 */ 
	void remove(T* x)
	{
		Slot* slot = (Slot*)((u8*)x - offsetof(Slot, element));
		slot->used = false;
		slot->next_free_slot = free_slot;
		free_slot = slot;
	}

	/* --------------------------------------------------------------------------------------------
	 *  Internal helpers
	 */ 
private:

	/* -------------------------------------------------------------------------------------------- new_chunk
	 */ 
	void new_chunk()
	{
		auto chunk = (Chunk*)mem.allocate(sizeof(Chunk));
		
		for (s32 i = 0; i < N_slots_per_chunk; i++)
		{
			Slot& slot = chunk->slots[i];
			slot.next_free_slot = (i == N_slots_per_chunk - 1 ? nullptr : &chunk->slots[i+1]);
			slot.used = false;
		}

		chunk->next = current_chunk;
		current_chunk = chunk;
	}
};


#endif
