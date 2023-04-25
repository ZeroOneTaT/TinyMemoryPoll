#define _CRT_SECURE_NO_WARNINGS 1
#include "PageCache.h"

// ��������
PageCache PageCache::_sInst;

// ��ȡһ��Kҳ��span
/*
1������k��Ͱ�����Ƿ���ڿ���span�������ھ�ֱ�ӷ��أ�
2���������ڣ���������Ͱ�����Ƿ���ڸ����span��
	���������з�һ��kҳ��span���أ�ʣ�µ�ҳ����span�ŵ���Ӧ��Ͱ�
3���������Ͱ��Ҳ�����ڿ���span������ϵͳ������һ����СΪ128ҳ��span��
	�������ŵ���Ӧ��Ͱ��ٵݹ�����Լ���ֱ����ȡ��һ��kҳ��span��
*/
Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0);
	// k > 128
	if (k > NPAGES - 1)
	{
		void* ptr = SystemAlloc(k);
		Span* span = _spanPool.New();

		span->_pageID = (PAGE_ID)ptr >> PAGE_SHIFT;		// ҳ�ţ���ַ����PAGE_SHIFT���
		span->_num = k;									// ҳ��


		 _idSpanMap[span->_pageID] = span;
		//_idSpanMap.set(span->_pageId, span);

		return span;
	}
	
	// 1������k��Ͱ�����Ƿ����span
	if (!_spanlists[k].Empty())
	{
		Span* kSpan = _spanlists[k].PopFront();

		// ����<id, span>ӳ�䣬����CentralCache�����ڴ�ʱ�����Ҷ�Ӧ��span
		for (PAGE_ID i = 0; i < kSpan->_num; ++i)
		{
			_idSpanMap[kSpan->_pageID + i] = kSpan;
		}

		return kSpan;
	}

	// 2����k��ͰΪ�գ�������Ͱ
	for (size_t i = k + 1; i < NPAGES; ++i)
	{
		if (!_spanlists[i].Empty())
		{
			Span* nSpan = _spanlists[i].PopFront();
			Span* kSpan = _spanPool.New();

			// �з�kSpan��С�ڴ棬ʣ����ص���Ӧӳ��λ��
			kSpan->_pageID = nSpan->_pageID;
			kSpan->_num = k;

			nSpan->_pageID += k;
			nSpan->_num -= k;

			// ʣ�ಿ�ַ����Ӧ��ϣͰ
			_spanlists[nSpan->_num].PushFront(nSpan);

			// �洢nSpan����βҳ�Ÿ�nSpanӳ�䣬 ����PageCache�����ڴ�ʱ���еĺϲ�����
			// ��Ϊû�����Ļ������ߣ�ֻ��Ҫ�����β
			 _idSpanMap[nSpan->_pageID] = nSpan;
			 _idSpanMap[nSpan->_pageID + nSpan->_num - 1] = nSpan;


			 for (PAGE_ID i = 0; i < kSpan->_num; ++i)
			 {
				 _idSpanMap[kSpan->_pageID + i] = kSpan;
			 }

			 return kSpan;
		}
	}
	// 3���޺���span���������һ��128ҳ��span
	Span* bigSpan = _spanPool.New();
	void* ptr = SystemAlloc(NPAGES - 1);

	// ͨ���� ptr ��ַǿ��ת��Ϊ PAGE_ID ���ͣ�
	// ��ʹ�ö�����λ����� >> ��ָ��ĵ�ַ���� PAGE_SHIFT λ
	// ���յõ��Ľ���������ָ�����ڵġ�ҳ�ı�š�
	bigSpan->_pageID = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->_num = NPAGES - 1;

	_spanlists[bigSpan->_num].PushFront(bigSpan);

	return NewSpan(k);// �ݹ�����Լ�����һ��һ���ܳɹ���

}


// ���ݴ����ָ���ַ�ҵ���Ӧӳ���Span��
// ���������˵�����ĵ�ַ�����⣨Ҫô�����߼�ʵ�ִ���Ҫô�����Ҵ���
Span* PageCache::MapObjectToSpan(void* obj)
{
	// ���ݵ�ַ���ҳ��
	PAGE_ID idx = (PAGE_ID)obj >> PAGE_SHIFT;
	
	// ע�⣺����span�����ܴ���������߳̽��в����ɾ����������Ҫ����
	// �Զ����������
	std::unique_lock<std::mutex> lock(_pageMtx);

	auto ret = _idSpanMap.find(idx);
	if (ret != _idSpanMap.end())
	{
		return ret->second;
	}
	else
	{
		assert(false);
		return nullptr;
	}
}

// ���ϻ���span���������Ϻ����½����ڴ�ϲ����������Ƭ����
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	// ����128ҳֱ�ӹ黹����
	if (span->_num > NPAGES - 1)
	{
		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		SystemFree(ptr);
		_spanPool.Delete(span);
		_idSpanMap.erase(span->_pageID);
		return;
	}

	// ��ǰ�ϲ�
	while (true)
	{
		PAGE_ID prevID = span->_pageID - 1;

		auto ret = _idSpanMap.find(prevID);
		// ǰ��spanΪ�գ����ϲ�
		if (ret == _idSpanMap.end())
			break;

		// ǰ��span��ʹ�ã����ϲ�
		Span* prevSpan = ret->second;
		if (prevSpan->_isUsed == true)
			break;

		// �ϲ��󳬹�128ҳ�����ϲ�
		if (prevSpan->_num + span->_num > NPAGES - 1)
			break;

		// ����ϲ�������ִ�кϲ�
		span->_pageID = prevSpan->_pageID;
		span->_num += prevSpan->_num;

		_spanlists[prevSpan->_num].Erase(prevSpan);
		_spanPool.Delete(prevSpan);

	}

	// ���ϲ�
	while (true)
	{
		PAGE_ID nextID = span->_pageID + span->_num;

		auto ret = _idSpanMap.find(nextID);
		// ����spanΪ�գ����ϲ�
		if (ret == _idSpanMap.end())
			break;

		// ǰ��span��ʹ�ã����ϲ�
		Span* nextSpan = ret->second;
		if (nextSpan->_isUsed == true)
			break;

		// �ϲ��󳬹�128ҳ�����ϲ�
		if (nextSpan->_num + span->_num > NPAGES - 1)
			break;

		// ����ϲ�������ִ�кϲ�
		span->_num += nextSpan->_num;

		_spanlists[nextSpan->_num].Erase(nextSpan);
		_spanPool.Delete(nextSpan);
	}

	_spanlists[span->_num].PushFront(span);			// �ϲ����span���¹��ص���Ӧ��˫������
	span->_isUsed = true;

	_idSpanMap[span->_pageID] = span;
	_idSpanMap[span->_pageID + span->_num - 1] = span;
}