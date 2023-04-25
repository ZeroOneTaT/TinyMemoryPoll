#pragma once
/*
	CentralCache���װ
*/

#include "Common.h"

// ����ʽ�ĵ������ģʽ:
// ���Ψһʵ���ڳ�������ʱ���Ѿ�������������
// ������������������������ڶ�ֻ����һ��ʵ��
// ����ʽ�ŵ����̰߳�ȫ����Ϊʵ���ڳ�������ʱ���Ѿ���������
// ��������������������ڶ�ֻ����һ��ʵ����������ڶ��߳̾����������

class CentralCache {
public:
	// ����ģʽ����ȡΨһʵ��
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}
	// ��ȡһ���ǿ�Span
	Span* GetOneSpan(SpanList& list, size_t byte_size);

	// ��CentralCacheȡһ�������ڴ�����ThreadCache
	size_t FetchRangeObjToThread(void*& start, size_t batchNum, size_t size);

	// ��һ�������Ķ����ͷŵ����Ļ����span���
	void ReleaseListToSpans(void* start, size_t byte_size);
private:
	SpanList _spanlists[NFREELIST];

private:
	// ���캯�����������캯��˽�л�
	CentralCache(){}

	CentralCache(const CentralCache&) = delete;

	// ���徲̬�ı��� _sInst�����ڱ��� CentralCache ���Ψһʵ��
	static CentralCache _sInst;
};