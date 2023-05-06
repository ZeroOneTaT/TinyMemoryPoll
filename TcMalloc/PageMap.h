#pragma once
/*
	����������ƣ�����ʹ���������Ķ��⿪��
*/
#include "Common.h"
//#include "MemoryPool.h"

// ���������
template <int BITS>
class PageMap1 {
private:
	// ��ҳ��ֱ��ӳ�䣬BITS = 32 - PAGE_SHIFT = 19λ
	// ��32λϵͳ���2^32λ�ֽڣ�ÿҳ2^PAGE_SHIFT�������2^BITSҳ
	static const int LENGTH = 1 << BITS;
	void** _array;
public:
	typedef uintptr_t Number;

	// ��ʽ���幹�캯������ֹ��ʽת��
	explicit PageMap1()
	{
		// �������鿪�ٿռ�����Ĵ�С
		size_t size = sizeof(void*) << BITS;
		size_t alignedSize = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT);
		// ֱ�����������ռ�
		_array = (void**)SystemAlloc(alignedSize >> PAGE_SHIFT);
		memset(_array, 0, size);
	}

	// ���ص�ǰkey��ֵ��δ�����򷵻�NULL
	void* get(Number key) const
	{
		if ((key >> BITS) > 0)
			return NULL;
		return _array[key];
	}

	// ����key-valueӳ��
	void set(Number key, void* value)
	{
		_array[key] = value;
	}
};


// ���������
template <int BITS>
class PageMap2 
{
private:
	// ��һ���������BITS<19λ>�ֽ��5 + 14λ���������.
	static const int ROOT_BITS = 5;
	static const int ROOT_LENGTH = 1 << ROOT_BITS;

	static const int LEAF_BITS = BITS - ROOT_BITS;
	static const int LEAF_LENGTH = 1 << LEAF_BITS;

	// Ҷ�ӽڵ㶨��
	struct Leaf 
	{
		void* values[LEAF_LENGTH];
	};

	Leaf* root_[ROOT_LENGTH];             // ���ڵ�ָ��Ҷ�ӽڵ�
	void* (*allocator_)(size_t);          // �ڴ������

public:
	typedef uintptr_t Number;

	explicit PageMap2() {
		memset(root_, 0, sizeof(root_));

		_preAllocatememory();
	}

	void* get(Number k) const {
		const Number i1 = k >> LEAF_BITS;
		const Number i2 = k & (LEAF_LENGTH - 1);
		if ((k >> BITS) > 0 || root_[i1] == NULL) {
			return NULL;
		}
		return root_[i1]->values[i2];
	}

	void set(Number k, void* v) {
		const Number i1 = k >> LEAF_BITS;
		const Number i2 = k & (LEAF_LENGTH - 1);
		assert(i1 < ROOT_LENGTH);
		root_[i1]->values[i2] = v;
	}

	bool _ensure(Number start, size_t n) {
		for (Number key = start; key <= start + n - 1;) {
			const Number i1 = key >> LEAF_BITS;

			// ����Ƿ�Խ��
			if (i1 >= ROOT_LENGTH)
				return false;

			// ���������ڵ�
			if (root_[i1] == NULL) {
				static	MemoryPool<Leaf>	leafPool;
				Leaf* leaf = (Leaf*)leafPool.New();

				memset(leaf, 0, sizeof(*leaf));
				root_[i1] = leaf;
			}

			// ��Ծ����һ�����ڵ�λ��
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		}
		return true;
	}

	void _preAllocatememory() {
		// Ԥ����ռ�
		_ensure(0, 1 << BITS);
	}
};

// ���������
// 64λϵͳʹ�ã�����Ŀ��Ҫ��32λϵͳ���У��ʲ��������������
template <int BITS>
class PageMap3 {
private:
	// How many bits should we consume at each interior level
	static const int INTERIOR_BITS = (BITS + 2) / 3; // Round-up
	static const int INTERIOR_LENGTH = 1 << INTERIOR_BITS;

	// How many bits should we consume at leaf level
	static const int LEAF_BITS = BITS - 2 * INTERIOR_BITS;
	static const int LEAF_LENGTH = 1 << LEAF_BITS;

	// Interior node
	struct Node {
		Node* ptrs[INTERIOR_LENGTH];
	};

	// Leaf node
	struct Leaf {
		void* values[LEAF_LENGTH];
	};

	Node* root_;                          // Root of radix tree
	void* (*allocator_)(size_t);          // Memory allocator

	Node* NewNode() {
		Node* result = reinterpret_cast<Node*>((*allocator_)(sizeof(Node)));
		if (result != NULL) {
			memset(result, 0, sizeof(*result));
		}
		return result;
	}

public:
	typedef uintptr_t Number;

	explicit PageMap3(void* (*allocator)(size_t)) {
		allocator_ = allocator;
		root_ = NewNode();
	}

	void* get(Number k) const {
		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
		const Number i3 = k & (LEAF_LENGTH - 1);
		if ((k >> BITS) > 0 ||
			root_->ptrs[i1] == NULL || root_->ptrs[i1]->ptrs[i2] == NULL) {
			return NULL;
		}
		return reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3];
	}

	void set(Number k, void* v) {
		ASSERT(k >> BITS == 0);
		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
		const Number i3 = k & (LEAF_LENGTH - 1);
		reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3] = v;
	}

	bool Ensure(Number start, size_t n) {
		for (Number key = start; key <= start + n - 1;) {
			const Number i1 = key >> (LEAF_BITS + INTERIOR_BITS);
			const Number i2 = (key >> LEAF_BITS) & (INTERIOR_LENGTH - 1);

			// Check for overflow
			if (i1 >= INTERIOR_LENGTH || i2 >= INTERIOR_LENGTH)
				return false;

			// Make 2nd level node if necessary
			if (root_->ptrs[i1] == NULL) {
				Node* n = NewNode();
				if (n == NULL) return false;
				root_->ptrs[i1] = n;
			}

			// Make leaf node if necessary
			if (root_->ptrs[i1]->ptrs[i2] == NULL) {
				Leaf* leaf = reinterpret_cast<Leaf*>((*allocator_)(sizeof(Leaf)));
				if (leaf == NULL) return false;
				memset(leaf, 0, sizeof(*leaf));
				root_->ptrs[i1]->ptrs[i2] = reinterpret_cast<Node*>(leaf);
			}

			// Advance key past whatever is covered by this leaf node
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		}
		return true;
	}

	void PreallocateMoreMemory() {
	}
};