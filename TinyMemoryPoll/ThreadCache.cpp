#define _CRT_SECURE_NO_WARNINGS 1
#include "ThreadCache.h"
#include "CentralCache.h"

// �����뻺��CentralCache��ȡ�ڴ��
// ��������������ThreadCache���������Ӧ��Ͱ���������ȡ���ڴ���С

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// ����ʼ���������㷨
	// 1���ʼ����һ����central cacheһ������Ҫ̫�࣬��ΪҪ̫���˿����ò���
	// 2������㲻Ҫ���size��С�ڴ�������ôbatchSize�ͻ᲻��������ֱ������
	// 3��sizeԽ��һ����central cacheҪ��batchSize��ԽС
	// 4��sizeԽС��һ����central cacheҪ��batchSize��Խ��
	size_t batchSize= min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
	if (_freeLists[index].MaxSize() == batchSize)
		++_freeLists[index].MaxSize();

	void* start = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObjToThread(start, batchSize, size);

	// ���ٻ�ȡһ��
	assert(actualNum > 0);

	if (1 == actualNum)
		assert(start == end);
	else     // ����鱣�浽_freeLists
		_freeLists[index].PushRange(NextObj(start), end, actualNum - 1);

	return start;
}

// �߳��ڷ����ڴ�
void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);

	// �����ڴ��Ķ����СalignSize���ڴ�����ڵ�����������±�index
	size_t alignSize = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);

	// _freeLists[index]�ǿգ�ֱ�� _freeLists[index]ȡ��һ���ڴ��
	if (!_freeLists[index].Empty())
		return _freeLists[index].Pop();
	// _freeLists[index]�ѿգ�����FetchFromCentralCache�����Ļ��������ڴ��
	else
		FetchFromCentralCache(index, alignSize);
}

// �߳��ڻ����ڴ�
// �������ڴ��ָ��ptr ���ڴ���Сsize
void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size <= MAX_BYTES);

	// ����ӳ�����������Ͱindex��������_freeLists[index]
	size_t index = SizeClass::Index(size);
	_freeLists[index].Push(ptr);

	// �������ȴ���һ������������ڴ�ʱ���Ϳ�ʼ��һ��list��CentralCache
	if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
		FreeToCentralCache(_freeLists[index], size);	
}

// ThreadCache�黹�ڴ�鵽CentralCache
void ThreadCache::FreeToCentralCache(FreeList& list, size_t size)
{
	void* start = nullptr;
	void* end = nullptr;

	list.PopRange(start, list.MaxSize());

	// ���ϲ�ռ䴫��
	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}