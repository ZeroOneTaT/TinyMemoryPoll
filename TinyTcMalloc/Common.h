#pragma once
#include <iostream>
#include <vector>
#include <atomic>
#include <thread>
#include <time.h>
#include <assert.h>
#include <mutex>
#include <unordered_map>
#include <algorithm>

using std::cout;
using std::endl;

#ifdef _WIN32
	#include <Windows.h>
#else
	// ......
#endif // _WIN32

#ifdef _WIN64
	typedef unsigned long long PAGE_ID;
#elif _WIN32
	typedef size_t PAGE_ID;
#else
	//Linux
#endif

static const size_t MAX_BYTES = 256 << 10;		// Thread Cache�д洢������ڴ��С 256 * 1024byte
static const size_t NFREELIST = 208;			// ThreadCache/CentralCache��ϣͰ�ĸ���
static const size_t NPAGES = 128;				// PageCache�е����ҳ����ӳ��Ͱ��������� 128 page = 1MB
static const size_t PAGE_SHIFT = 13;			// �����һҳ�ڴ��С 8 * 1024kb = 2^13 byte

// �ڴ�ָ��ת�� 4/32λ 8/64λ
static inline void*& NextObj(void* obj) 
{
	return *(void**)obj;
}

// �Զ����ڴ����뺯��
inline static void* SystemAlloc(size_t kPage)
{
	#ifdef _WIN32
		void* ptr = VirtualAlloc(0, kPage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	#else
		//Linux��brk mmap��
	#endif // _WIN32

	//�׳��쳣
	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

// �Զ����ͷ��ڴ溯��
inline static void SystemFree(void* ptr)
{
	#ifdef _WIN32
		VirtualFree(ptr, 0, MEM_RELEASE);
	#else
		//sbrk unmmap��
	#endif // _WIN32

}

// �������������зֺõĶ���
class FreeList {
public:
	//���黹���ڴ�����ͷ�����������
	void Push(void* obj)
	{
		NextObj(obj) = _freeList;
		_freeList = obj;
		++_size;
	}
	// ����һ���ڴ�
	void PushRange(void* start, void* end, size_t size)
	{
		NextObj(end) = _freeList;
		_freeList = start;
		_size += size;
	}
	// �����������е��ڴ��ͷɾ��ȥ
	void* Pop()
	{
		assert(_freeList);
		void* obj = _freeList;
		_freeList = NextObj(obj);
		--_size;
		return obj;
	}
	// ɾ��һ���ڴ�
	void PopRange(void*& start, size_t n)
	{
		assert(n >= _size);
		start = _freeList;
		void* end = _freeList;
		for (size_t i = 0; i < n - 1; i++)
		{
			end = NextObj(end);
		}
		_freeList = NextObj(end);
		_size -= n;
		NextObj(end) = nullptr;
	}

	bool Empty()
	{
		return _freeList == nullptr;
	}
	size_t& MaxSize()
	{
		return _maxSize;
	}
	size_t& Size()
	{
		return _size;
	}
private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;			//���������ڱ�ס������������
	size_t _size = 0;				//������
};


// ��������ӳ���ϵ
class SizeClass {
public:
	// ������������10%���ҵ�����Ƭ�˷�
	// [1,128]					8byte����			freelist[0,16)			��Ƭ�� (8-1)/8
	// [128+1,1024]				16byte����			freelist[16,72)			��Ƭ�� (128+16-129)/(128+16)
	// [1024+1,8*1024]			128byte����			freelist[72,128)		��Ƭ�� (1024+128-1025)/(1024+128)
	// [8*1024+1,64*1024]		1024byte����		freelist[128,184)		��Ƭ�� (8*1024+1024-8*1024+1)/(8*1024+1024)
	// [64*1024+1,256*1024]		8*1024byte����		freelist[184,208)		��Ƭ�� (64*1024+8*1024-64*1024+1)/(64*1024+8*1024)

	// λ���㽫size���뵽>=size��alignNum�ı���
	static inline size_t _RoundUp(size_t bytes, size_t alignNum)
	{
		return (bytes + alignNum - 1) & ~(alignNum - 1);
	}
	static inline size_t RoundUp(size_t size)
	{
		if (size <= 128)
			return _RoundUp(size, 8);
		else if (size <= 1024)
			return _RoundUp(size, 16);
		else if (size <= 8 * 1024)
			return _RoundUp(size, 128);
		else if (size <= 64 * 1024)
			return _RoundUp(size, 1024);
		else if (size <= 256 * 1024)
			return _RoundUp(size, 8 * 1024);
		else
			return _RoundUp(size, 1 << PAGE_SHIFT);
	}

	// �� size ӳ�䵽Ͱ�����±꣺
	// ���㷽���ǽ� size ���϶��뵽��ӽ����Ĵ��ڵ������� 2^align_shift(��alignNum) ��"����"��Ȼ�����ټ�ȥ 1��
	// ������������ú� _RoundUp �������ƣ����������ص����±�����Ƕ�����ֵ��
	// ����size = 11ӳ���±굽(2 - 1 = 1) 
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}
	// ����ӳ�䵽�ĸ�����Ͱ
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);

		static int group_array[4] = { 16, 56 + 16, 56 + 56 + 16, 56 + 56 + 56 + 16 };
		if (bytes <= 128)
			return _Index(bytes, 3);					// 2^3
		else if (bytes <= 1024)
			return _Index(bytes, 4) + group_array[0];	// 2^4
		else if (bytes <= 8 * 1024)
			return _Index(bytes, 7) + group_array[1];	// 2^7
		else if (bytes <= 64 * 1024)
			return _Index(bytes, 10) + group_array[2];	// 2^10
		else if (bytes <= 256 * 1024)
			return _Index(bytes, 13) + group_array[3];	// 2^3
		else
			assert(false);

		return -1;
	}

	// �ڴ治������CentralCache����
	// ����ThreadCacheһ�δ����Ļ���CentralCache��ȡ���ٸ�С�����ܵĴ�С����MAX_BYTES = 256KB
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);

		// [2, 512], һ�������ƶ����ٸ����������ֵ����������
		// �������μ����ȡ����������[1,32768]����Χ����
		// ��˿��ƻ�ȡ�Ķ���������Χ������[2, 512],��Ϊ����
		// С�����ȡ�����ζ࣬������ȡ��������
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;
		return num;
	}

	// �������Ļ���CentralCacheһ����PageCache��ȡ����ҳ
	// �������� 8byte
	// ...
	// �������� 256KB
	static size_t NumMovePage(size_t size)
	{
		// ����һ�δ����Ļ����ȡ�Ķ������num
		size_t num = NumMoveSize(size);
		// ���������С�����������,���һ����Ҫ��PageCache������ڴ��С
		size_t n_page = num * size;

		n_page >>= PAGE_SHIFT;
		if (0 == n_page)
			n_page = 1;

		return n_page;
	}
private:
	//
};


// CentralCache����������ҳ����ڴ��Ƚṹ
struct Span
{
	PAGE_ID _pageID = 0;	// ���ڴ���ʼҳid
	size_t _num = 0;		// ҳ����

	Span* _prev = nullptr;
	Span* _next = nullptr;

	size_t _objSize = 0;	// Span�з�С���ڴ��С
	bool _isUsed = false;	// Span�Ƿ�ʹ�ñ�־
	size_t _usedCount = 0;	// Span�ѷ����ThreadCache���ڴ�����
	void* _freeList = nullptr;
};

// Span��ͷ˫������ṹ
class SpanList
{
public:
	SpanList()
	{
		_head = new Span();
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* Begin()
	{
		return _head->_next;
	}

	Span* End()
	{
		return _head;
	}

	bool Empty()
	{
		return _head->_next == _head;
	}

	// ������ָ��λ�ò����µ��ڴ��
	void Insert(Span* pos, Span* newSpan)
	{
		assert(pos);
		assert(newSpan);

		Span* prev = pos->_prev;
		prev->_next = newSpan;
		newSpan->_next = pos;
		newSpan->_prev = prev;
		pos->_prev = newSpan;
	}
	
	// ��������ɾ��ָ���ڴ��
	void Erase(Span* pos)
	{
		assert(pos);
		// ָ������ͷΪ�Ƿ�λ��
		assert(pos != _head);

		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;
	}

	// ͷ��
	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}

	// ͷɾ��������ɾ���Ľ��ָ�룻������������ɾ�������ǰѹ�ϵ�����
	Span* PopFront()
	{
		Span* front = _head->_next;
		Erase(front);
		return front;
	}

	// Ͱ������
	void lock() { _mtx.lock(); }
	// Ͱ������
	void unlock() { _mtx.unlock(); }

private:
	Span* _head;		// �����ͷָ��

	std::mutex _mtx;	// Ͱ��,�����̰߳�ȫ
};