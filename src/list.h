/*
 *  Generic linked list structures
 */

#ifndef _lake_list_h
#define _lake_list_h

#include "pool.h"

/* ================================================================================================ SList
 *  Singly linked list.
 */
template<typename T>
struct SList
{
	/* ============================================================================================ SList::Node
	*/
	struct Node
	{
		T*    data;
		Node* next;

		operator T&() { return *data; }
	};

	Pool<Node> pool;

	Node* head;


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */


	/* -------------------------------------------------------------------------------------------- create
	 */
	static SList<T> create()
	{
		SList<T> out = {};
		out.pool = Pool<Node>::create();
		out.head = nullptr;
		return out;
	}

	/* -------------------------------------------------------------------------------------------- destroy
	 */
	void destroy()
	{
		pool.destroy();
		head = nullptr;
	}

	/* -------------------------------------------------------------------------------------------- push
	 */
	Node* push(T* x)
	{
		Node* n = pool.add();
		n->next = head;
		n->data = x;
		head = n;
		return n;
	}

	/* -------------------------------------------------------------------------------------------- pop
	 */
	T* pop()
	{
		Node* n = head;
		head = n->next;

		T* data = n->data;
		pool.remove(n);
		return data;
	}

	/* -------------------------------------------------------------------------------------------- remove_next
	 *  Removes the node following the one given from the list
	 */
	void remove_next(Node* n)
	{
		Node* save = n->next;
		n->next = save->next;
		pool.remove(n);
	}	

	/* -------------------------------------------------------------------------------------------- is_empty
	 */
	b8 is_empty()
	{
		return !head;
	}

	/* -------------------------------------------------------------------------------------------- copy
	 *  Returns a copy of this list.
	 */
	SList<T> copy()
	{
		SList<T> out = SList<T>::create();

		if (!head)
			return out;

		Node* old_iter = head;
		Node* new_iter = out.push(head->data);
		
		while (old_iter->next)
		{
			old_iter = old_iter->next;
			Node* n = out.pool.add();
			n->data = old_iter->data;
			new_iter->next = n;
			new_iter = n;
		}

		return out;
	}
};

/* ================================================================================================ DList
 *  Doubly linked list
 */
template<typename T>
struct DList
{

	/* ============================================================================================ DList::Node
	*/
	struct Node 
	{
		T*    data;
		Node* next;
		Node* prev;
	};

	Pool<Node> pool;

	Node* head;
	Node* tail;
	

	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */


	/* -------------------------------------------------------------------------------------------- create
	 */
	static DList<T> create()
	{
		DList<T> out = {};
		out.pool = Pool<Node>::create();
		out.head = out.tail = nullptr;
		return out;
	}

	/* -------------------------------------------------------------------------------------------- create
	 */
	void destroy()
	{
		pool.destroy();
		head = tail = nullptr;
	}

	/* -------------------------------------------------------------------------------------------- front
	 */
	T* front()
	{
		return head? head->data : nullptr;
	}

	/* -------------------------------------------------------------------------------------------- back
	 */
	T* back()
	{
		return tail? tail->data : nullptr;
	}

	/* -------------------------------------------------------------------------------------------- is_empty
	 */
	b8 is_empty()
	{
		return !head;
	}

	/* -------------------------------------------------------------------------------------------- insert_after
	 */
	Node* insert_after(Node* x, T* t = nullptr)
	{
		Node* n = pool.add();
		n->prev = x;
		if (x->next) 
		{
			x->next->prev = n;
			n->next = x->next;
		}
		else
			tail = n;
		x->next = n;
		n->data = t;
		return n;
	}

	/* -------------------------------------------------------------------------------------------- insert_before
	 */
	Node* insert_before(Node* x, T* t = nullptr)
	{
		Node* n = pool.add();
		n->next = x;
		if (x->prev)
		{
			x->prev->next = n;
			n->prev = x->prev;
		}
		else
			head = n;
		x->prev = n;
		n->data = t;
		return n;
	}

	/* -------------------------------------------------------------------------------------------- push_head
	 */
	Node* push_head(T* x = nullptr)
	{
		if (head)
			return insert_before(head, x);

		head = pool.add();
		tail = head;
		head->data = x;
		return head;
	}

	/* -------------------------------------------------------------------------------------------- push_tail
	 */
	Node* push_tail(T* x = nullptr)
	{
		if (tail)
			return insert_after(tail, x);
		return push_head(x);
	}

	/* -------------------------------------------------------------------------------------------- remove
	 */
	void remove(Node* x)
	{
		if (x->prev)
			x->prev->next = x->next;
		else
			head = x->next;

		if (x->next)
			x->next->prev = x->prev;
		else
			tail = x->prev;
	}	

	/* -------------------------------------------------------------------------------------------- destroy
	 *  removes the given node from the list and also deletes it from the pool
	 */
	void destroy(Node* x)
	{
		remove(x);
		pool.remove(x);
	}

	/* -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ RangeIterator 
	 *  Iterator for compatibility with C++ ranged for loops 
	 */
	struct RangeIterator
	{
		Node* n;

		Node* operator++()
		{
			n = n->next;
			return n;
		}

		b8 operator !=(const RangeIterator& rhs)
		{
			return n != rhs.n;
		}

		T* operator->()
		{
			return n->data;
		}

		T& operator*()
		{
			return *n->data;
		}
	};


	RangeIterator begin() 
	{
		return RangeIterator{head};
	}

	RangeIterator end() 
	{
		return RangeIterator{nullptr};
	}
};


/* ================================================================================================ DListIterator  
 */
template<typename T>
struct DListIterator
{
	DList<T>::Node* current;

	
	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */


	/* -------------------------------------------------------------------------------------------- DListIterator<T>()
	 */
	DListIterator<T>(DList<T>& list) 
	{
		current = list.head;
	}

	/* -------------------------------------------------------------------------------------------- DListIterator<T>()
	 */
	T* next()
	{
		if (!current)
			return nullptr;

		T* out = current->data;
		current = current->next;
		return out;
	}

	/* -------------------------------------------------------------------------------------------- operator->
	 */
	T* operator->()
	{
		return current->data;
	}


	/* -------------------------------------------------------------------------------------------- operator bool
	 */
	operator bool()
	{
		return current;
	}
};

#endif
