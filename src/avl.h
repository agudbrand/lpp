/*
 *  AVL tree implementation
 */

#ifndef _lpp_avl_h
#define _lpp_avl_h

#include "common.h"
#include "pool.h"
#include "unicode.h"

/* ================================================================================================ AVL
 */
template<

	// the type this tree points at
	typename T,

	// key accessor
	// which could be a hash function or member or whatever
	u64 (*GetKey)(const T*)
	
>
struct AVL
{
	typedef AVL<T, GetKey> This;

	/* ============================================================================================ AVL::Node
 	*/
	struct Node
	{
		s8 balance_factor = 0; // -1, 0, or 1
		// TODO(sushi) we may be able to do something neat where we instead store offsets from the nodes position in the 
		//             pool with s32s or even like s16 to save some space so experiment later
		//             this would require the pools chunks being connected via doubly connected list
		Node* left = nullptr;
		Node* right = nullptr;
		Node* parent = nullptr;
		T* data = nullptr;
	};
	
	Node* root;

	Pool<Node> pool;


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */


	/* -------------------------------------------------------------------------------------------- create
	 */
	static This create()
	{
		This out = {};
		out.pool = Pool<Node>::create();
		return out;
	}

	/* -------------------------------------------------------------------------------------------- destroy
	 */
	void destroy()
	{
		pool.destroy();
		root = nullptr;
	}

	/* -------------------------------------------------------------------------------------------- has
	 */
	b8 has(T* data)
	{
		if (!root)
			return false;

		u64 data_key = GetKey(data);

		Node* search_node = root;
		for (;;)
		{
			u64 search_key = GetKey(search_node->data);

			if (search_key == data_key)
				return true;

			if (search_key > data_key)
			{
				if (search_node->left)
					search_node = search_node->left;
				else
					return false;
			}
			else if (search_node->right)
				search_node = search_node->right;
			else
				return false;
		}
	}

	/* -------------------------------------------------------------------------------------------- find
	 */
	T* find(u64 key)
	{
		if (!root)
			return nullptr;

		Node* search_node = root;
		for (;;)
		{
			u64 search_key = GetKey(search_node->data);

			if (search_key == key)
				return search_node->data;

			if (search_key > key)
			{
				if (search_node->left)
					search_node = search_node->left;
				else
					return nullptr;
			}
			else if (search_node->right)
				search_node = search_node->right;
			else
				return nullptr;
		}
	}


	/* -------------------------------------------------------------------------------------------- insert
	 *  Attempts to insert the given data. If the data already exists then nothing happens.
	 */
	void insert(T* data)
	{
		if (!root)
		{
			root = pool.add();
			root->data = data;
			root->balance_factor = 0;
			return;
		}

		u64 data_key = GetKey(data);
		
		Node* search_node = root;
		b8 place_right = true;
		for (;;)
		{
			u64 search_key = GetKey(search_node->data);

			if (search_key == data_key)
				return;
			
			if (search_key > data_key)
			{
				if (search_node->left)
				{
					search_node = search_node->left;
				}
				else
				{
					place_right = false;
					break;
				}
			}
			else if (search_node->right)
				search_node = search_node->right;
			else
				break; 
		}

		// need a new node
		Node* new_node = pool.add();
		new_node->parent = search_node;
		if (place_right)
			search_node->right = new_node;
		else
			search_node->left = new_node;

		new_node->data = data;

		// walk back up the tree and adjust balance
		for (Node* parent = search_node,* child = new_node; parent; child = parent, parent = child->parent)
		{
			Node* new_child = nullptr;
			Node* parents_parent = parent->parent;

			if (parent->right == child)
			{
				// right subtree has increased in height
				if (parent->balance_factor > 0)
				{
					// the parent was already right heavy, so its balance factor
					// is now +2
					if (child->balance_factor < 0)
						new_child = rotate_right_left(parent, child);
					else 
						new_child = rotate_left(parent, child);
				}
				else
				{
					// the parent is either already balanced or left heavy
					if (parent->balance_factor < 0)
					{
						// if left heavy, then we're now balanced by the
						// addition to the right child.
						parent->balance_factor = 0;
						break;
					}
					// if we were balanced all we need to do is set the parent's 
					// balance factor to 1 and move on
					parent->balance_factor = 1;
					continue;
				}
			}
			else
			{
				if (parent->balance_factor < 0)
				{
					if (child->balance_factor > 0)
						new_child = rotate_left_right(parent, child);
					else
						new_child = rotate_right(parent, child);
				}
				else
				{
					if (parent->balance_factor > 0)
					{
						parent->balance_factor = 0;
						break;
					}
					parent->balance_factor = -1;
					continue;
				}
			}

			new_child->parent = parents_parent;
			if (parents_parent) 
			{
				if (parent == parents_parent->left)
					parents_parent->left = new_child;
				else
					parents_parent->right = new_child;
			}
			else
				root = new_child;
			break;
		}
	}
	
	/* -------------------------------------------------------------------------------------------- remove
	 *  Removes the given data from the tree.
	 */
	void remove(T* data)
	{
		u64 data_key = GetKey(data);

		// find the node representing the given data
		Node* search_node = root;
		for (;;)
		{
			if (!search_node)
				return;

			u64 search_key = GetKey(search_node->data);

			if (search_key == data_key)
				break;

			if (data_key > search_key)
				search_node = search_node->right;
			else
				search_node = search_node->left;
		}

		Node* target = search_node;

		if (search_node->left && search_node->right)
		{
			target = predecessor(search_node);
			search_node->data = target->data;
		}

		for (Node* parent = target->parent,* child = target; parent; parent = child->parent)
		{
			Node* parents_parent = parent->parent;
			u8 b = 0;

			if (child == parent->left)
			{
				// left subtree decreases
				if (parent->balance_factor > 0)
				{
					// parent was right heavy
					// so we need to rebalance it 
					Node* sibling = parent->right;
					b = sibling->balance_factor;
					if (b < 0)
						child = rotate_right_left(parent, sibling);
					else
						child = rotate_left(parent, sibling);
				}
				else
				{
					if (parent->balance_factor == 0)
					{
						parent->balance_factor = 1;
						break;
					}
					child = parent;
					child ->balance_factor = 0;
					continue;
				}
			}
			else
			{
				if (parent->balance_factor < 0)
				{
					Node* sibling = parent->left;
					b = sibling->balance_factor;
					if (b < 0)
						child = rotate_left_right(parent, sibling);
					else
						child = rotate_right(parent, sibling);
				}
				else
				{
					if (parent->balance_factor == 0)
					{
						parent->balance_factor = -1;
						break;
					}
					child = parent;
					child ->balance_factor = 0;
					continue;
				}
			}

			child ->parent = parents_parent;
			if (parents_parent)
			{
				if (parent == parents_parent->left)
					parents_parent->left = child ;
				else
					parents_parent->right = child ;
			}
			else
				root = child;

			if (!b)
				break;
		}

		Node* child = (target->left ? target->left : target->right);

		if (child)
			child->parent = target->parent;

		if (target->parent)
		{
			if (target == target->parent->left)
				target->parent->left = child;
			else
				target->parent->right = child;
		}

		pool.remove(target);
	}

	/* -------------------------------------------------------------------------------------------- replace
	 *  Replaces u with v
	 */
	void replace(Node* u, Node* v)
	{
		if (!u->parent)
			root = v;
		else if (u == u->parent->left)
			u->parent->left = v;
		else
			u->parent->right = v;
		if (v)
			v->parent = u->parent;
	}

	/* -------------------------------------------------------------------------------------------- successor
	 */
	static Node* successor(Node* n)
	{
		if (n->right)
			return minimum(n->right);
		Node* parent = n->parent;
		while (parent && n == parent->right)
		{
			n = parent;
			parent = parent->parent;
		}
		return parent;
	}

	/* -------------------------------------------------------------------------------------------- predecessor
	 */
	static Node* predecessor(Node* n)
	{
		if (n->left)
			return maximum(n->left);
		Node* parent = n->parent;
		while (parent && n == parent->left)
		{
			n = parent;
			parent = parent->parent;
		}
		return parent;
	}

	/* -------------------------------------------------------------------------------------------- maximum
	 */ 
	static Node* maximum(Node* n)
	{
		while (n->right)
			n = n->right;
		return n;
	}

	/* -------------------------------------------------------------------------------------------- minimum
	 */ 
	static Node* minimum(Node* n)
	{
		while (n->left)
			n = n->left;
		return n;
	}

	/* -------------------------------------------------------------------------------------------- to_string
	 */
	template<void (*callback)(dstr& out, Node* node)>
	dstr to_string()
	{
		dstr out = dstr::create();
	
		if (!root)
		{
			out.append("X\n");
			return out;
		}
			
		to_string_recursive<callback>(out, root, 0);

		out.append("\n");

		return out;
	}

	/* --------------------------------------------------------------------------------------------
	 *  Internal helpers
	 */
private:

	template<void (*callback)(dstr& out, Node* node)>
	void to_string_recursive(dstr& out, Node* n, u32 layers)
	{
		for (s32 i = 0; i < layers; i++) out.append("  ");

		if (n->right || n->left)
			out.append("(");
		else
		{
			callback(out, n);
			return;
		}

		callback(out, n);
		out.appendv("\n");

		if (n->left)
			to_string_recursive<callback>(out, n->left, layers + 1);
		else
		{
			for (s32 i = 0; i < layers + 1; i++) out.append("  ");
			out.append("X");
		}

		out.append("\n");
		if (n->right)
			to_string_recursive<callback>(out, n->right, layers + 1);
		else
		{
			for (s32 i = 0; i < layers + 1; i++) out.append("  ");
			out.append("X");
		}

		if (n->right || n->left)
			out.append(")");
	}

	/* -------------------------------------------------------------------------------------------- rotate_left
	 *  (A a (B b g))
	 *  ->
	 *  (B (A a b) g)
	 */
	Node* rotate_left(Node* A, Node* B)
	{
		Node* b = B->left;
		A->right = b;
		if (b)
			b->parent = A;
		B->left = A;
		A->parent = B;

		if (!B->balance_factor)
		{
			// in if B was balanced, it no longer is
			// this only happens in deletion
			A->balance_factor =  1;
			B->balance_factor = -1;
		}
		else
		{
			// otherwise the tree has been rebalanced.
			A->balance_factor = 
			B->balance_factor = 0;
		}

		return B;
	}

	/* -------------------------------------------------------------------------------------------- rotate_right
	 *  (A (B a b) g)
	 *  ->
	 *  (B a (A b g))
	 */
	Node* rotate_right(Node* A, Node* B)
	{
		Node* b = B->right;
		A->left = b;
		if (b)
			b->parent = A;
		B->right = A;
		A->parent = B;

		if (!B->balance_factor)
		{
			A->balance_factor = -1;
			B->balance_factor =  1;
		}
		else
		{
			B->balance_factor = 
			A->balance_factor = 0;
		}

		return B;
	}
	
	/* -------------------------------------------------------------------------------------------- rotate_right_left
	 *  (A a (C (B b c) d))
	 *  -> 
	 *  (A a (B b (C c d)))
	 *  ->
	 *  (B (A a b) (C c d))
	 */
	Node* rotate_right_left(Node* A, Node* C)
	{
		Node* B = C->left;
		Node* c = B->right;
		C->left = c;
		if (c)
			c->parent = C;
		B->right = C;
		C->parent = B;
		Node* b = B->left;
		A->right = b;
		if (b)
			b->parent = A;
		B->left = A;
		A->parent = B;

		if (!B->balance_factor)
		{
			A->balance_factor = 0;
			C->balance_factor = 0;
		}
		else if (B->balance_factor > 0)
		{
			A->balance_factor = -1;
			C->balance_factor =  0;
		}
		else
		{
			A->balance_factor =  0;
			C->balance_factor = -1;
		}

		return B;
	}

	/* -------------------------------------------------------------------------------------------- rotate_left_right
	 *  (A (C a (B b c)) d)
	 *  ->
	 *  (A (B (C a b) c) d)
	 *  ->
	 *  (B (C a b) (A c d))
	 */
	Node* rotate_left_right(Node* A, Node* C)
	{
		Node* B = C->right;
		Node* b = B->left;
		C->right = b;
		if (b) 
			b->parent = C;
		B->left = C;	
		C->parent = B;
		Node* c = B->right;
		A->left = c;
		if (c)
			c->parent = A;
		B->right = A;
		A->parent = B;

		if (!B->balance_factor)
		{
			A->balance_factor = 0;
			C->balance_factor = 0;
		}
		else if (B->balance_factor > 0)
		{
			A->balance_factor = -1;
			C->balance_factor =  0;
		}
		else
		{
			A->balance_factor =  0;
			C->balance_factor = -1;
		}

		return B;
	}

private:

	/* -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ RangeIterator 
	 *  Iterator for compatibility with C++ ranged for loops 
	 *
	 *  It is NOT safe to add or remove things from the tree while using this!!
	 */
	struct RangeIterator
	{
		Node* current;

		Node* operator++()
		{
			current = successor(current);
			return current;
		}

		b8 operator !=(const RangeIterator& rhs)
		{
			return current != rhs.current;
		}

		T* operator->()
		{
			return current;
		}

		T& operator*()
		{
			return *current->data;
		}
	};

public:

	RangeIterator begin()
	{
		return RangeIterator{ root? minimum(root) : nullptr };
	}

	RangeIterator end()
	{
		return RangeIterator{ nullptr };
	}
};


#endif
