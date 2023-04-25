#pragma once
/*
	�ⲿ�����ڴ桢�ͷ��ڴ溯����װ
*/

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "MemoryPool.h"

// �̹߳���TreadCache��
static MemoryPool<ThreadCache> tcPool;

// �ڴ�����ӿ�
static void* TcMalloc(size_t size)
{
	// �������256k���ڴ棬ֱ����PageCache����
	if (size > MAX_BYTES)
	{
		size_t alignSize = SizeClass::RoundUp(size);
		size_t kPage = alignSize >> PAGE_SHIFT;

		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kPage);
		span->_objSize = alignSize;
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		return ptr;
	}
	// ��С�ڴ棬����������
	else
	{
		// ��鵱ǰ�߳��Ƿ��ж�Ӧ��ThreadCache�������û�У���ͨ��TLS ÿ���߳������Ļ�ȡ�Լ���ר����ThreadCache����
		if (pTLSThreadCache == nullptr)
		{
			// pTLSThreadCache = new ThreadCache;
			pTLSThreadCache = tcPool.New();
		}

		 cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl;

		// ���ø��̵߳�ThreadCache�����Allocate���������ڴ�
		return pTLSThreadCache->Allocate(size);
	}
}


// �ڴ��ͷŽӿ�
static void TcFree(void* ptr)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;
	// ����256k�ڴ棬ֱ���ͷŸ�PageCache
	if (size > MAX_BYTES)
	{
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	// ��С�ڴ棬����������
	else
	{
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}