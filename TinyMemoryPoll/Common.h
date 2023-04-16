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

static const size_t MAX_BYTE = 256 << 10;		// Thread Cache�д洢������ڴ��С 256 * 1024byte
static const size_t NFREELIST = 208;			// ThreadCache/CentralCache��ϣͰ�ĸ���
static const size_t NPAGES = 128;				// PageCache�е����ҳ����ӳ��Ͱ��������� 128 page = 1MB
static const size_t PAGE_SHIFT = 13;			// �����һҳ�ڴ��С 8 * 1024kb = 2^13 byte

// �ڴ�ָ��ת�� 4/32λ 8/32λ
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