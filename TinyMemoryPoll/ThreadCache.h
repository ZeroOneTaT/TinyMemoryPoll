#pragma once
/*
	ThreadCache��װ
	Ϊÿһ���߳��ṩһ��������ThreadCache
*/
#include "Common.h"

class ThreadCache
{
public:
	// ������ͷ��ڴ����
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);

	// �ڴ治�㣬��CentralCache��ȡ�ڴ����
	void* FetchFromCentralCache(size_t index, size_t size);

	// �ͷ��ڴ�ʱ����������������������ڴ浽CentralCache
	void FreeToCentralCache(FreeList& list, size_t size);
private:
	// ��ϣͰ��ÿ��Ͱ�й��ڶ�Ӧ��С�������������
	FreeList _freeLists[NFREELIST];
};

// pTLSThreadCache��һ��ָ��ThreadCache�����ָ�룬ÿ���̶߳���һ��������pTLSThreadCache
// ����ʹ�߳�����thread cache�����ڴ�����ʱ��ʵ����������
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;