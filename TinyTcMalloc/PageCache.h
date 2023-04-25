#pragma once
/*
	PageCache���װ
*/

#include "Common.h"
#include "MemoryPool.h"

class PageCache {
public:
	// ����ʽ�������ģʽ
	static PageCache* GetInstance()
	{
		return &_sInst;
	}
	
	// ��ȡ���ڴ����span��ӳ��
	Span* MapObjectToSpan(void* obj);

	// �ͷſ���span��PageCache�����ϲ�����span
	void ReleaseSpanToPageCache(Span* span);

	// ����kҳ��span
	Span* NewSpan(size_t k);

	void lock() { _pageMtx.lock(); }
	void unlock() { _pageMtx.unlock(); }

private:
	std::mutex _pageMtx;							// ��

	static PageCache _sInst;

	SpanList _spanlists[NPAGES];					//	PageCache��˫�����ϣͰ��ֱ�Ӱ�ҳ��ӳ��
	MemoryPool<Span> _spanPool;

	std::unordered_map<PAGE_ID, Span*> _idSpanMap;	// 

	PageCache(){}
	PageCache(const PageCache&) = delete;
};