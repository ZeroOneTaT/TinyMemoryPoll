#define _CRT_SECURE_NO_WARNINGS 1
#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst;	// ����ģʽ����

// ��CentralCacheȡ����Span,���ޣ���PageCache����
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freeList != nullptr)
			return it;
		else
			it = it->_next;
	}

	// ���������������߳��ͷŶ���
	list.unlock();

	// �޿���Span����PageCache����
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUsed = true;
	span->_objSize = size;
	PageCache::GetInstance()->_pageMtx.unlock();

	// ���»�ȡspan�з֣��������������ģʽ�������߳��޷����ʸ�span

	// ����span����ڴ����ʼ��ַ����С(�ֽ�)
	char* start = (char*)(span->_pageID << PAGE_SHIFT);
	size_t bytes = span->_num << PAGE_SHIFT;
	char* end = start + bytes;

	// �зֲ�����
	span->_freeList = start;
	start += size;
	void* tail = span->_freeList;
	int i = 1;
	while (start < end)
	{
		++i;
		NextObj(tail) = start;
		tail = NextObj(tail);
		start += size;
	}
	NextObj(tail) = nullptr;

	// �з�span�󣬼������ص�CentralCache��Ӧ�Ĺ�ϣͰ��
	list.lock();
	list.PushFront(span);

	return span;
}

// ��CentralCache��ȡһ�������ڴ�����ThreadCache
size_t CentralCache::FetchRangeObjToThread(void*& start, size_t batchNum, size_t size)
{
	// CentralCache��ϣͰ��ӳ������ThreadCache��ϣͰӳ�����һ��
	size_t index = SizeClass::Index(size);
	// ����
	_spanlists[index].lock();
	Span* span = GetOneSpan(_spanlists[index], size);
	assert(span);					// ����ȡ��span�Ƿ�Ϊ��
	assert(span->_freeList);		// ����ȡ��span�����������Ƿ�Ϊ��

	// ���뵽��span,�������з�
	void* end = span->_freeList;
	start = end;
	assert(start);

	size_t actualNum = 1;			// ʵ������
	while (actualNum < batchNum && NextObj(end))
	{
		++actualNum;
		end = NextObj(end);
	}

	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	span->_usedCount += actualNum;

	// ����
	_spanlists[index].unlock();
	return actualNum;
}

// ����ThreadCache���ص���������������ӳ���Ӧ��span����
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	// ����start��ѯ��Ӧ��span
	size_t index = SizeClass::Index(size);

	_spanlists[index].lock();
	
	while (start)
	{
		// ��start��ͷ����һ�����������ڴ�黹��Ӧspan,һ��ѭ����һ����һֱ��
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
		void* next = NextObj(start);
		// ͷ��
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_usedCount--;

		// span���зֳ�ȥ������С���ڴ�ȫ�����ѣ�����span��������span����page
		// ���span�Ϳ����ٻ��ո�PageCache��PageCache�����ٳ���ȥ��ǰ��ҳ�ĺϲ�
		if (span->_usedCount == 0)
		{
			// �ȴ�spanlists��ɾ��span
			_spanlists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = span->_prev = nullptr;

			_spanlists[index].unlock();

			// �ͷ�span��page cacheʱ��span�Ѿ���_spanLists[index]ɾ���ˣ�����Ҫ�ټ�Ͱ����
			// ��ʱ��Ͱ�������ʹ��page cache�����Ϳ�����,���������߳�����/�ͷ��ڴ�
			_spanlists[index].unlock();

			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();

			_spanlists[index].lock();
		}
		start = next;
	}
	_spanlists[index].unlock();
}