#pragma once

#include "Common.h"

// �����ڴ����
template <class T> 
class MemoryPool 
{
public:
	// �ض���New
	T* New() 
	{
		T* obj = nullptr;
		// ��������ǿգ���"ͷɾ"��ʽ����������ȡ���ڴ�飬�ظ�����
		if (_freeList != nullptr) 
		{
			obj = (T*)_freeList;
			//_freeList = *(void**)_freeList;
			_freeList = NextObj(_freeList);
		}
		else 
		{
			// ˵������������û�н��
			// �ж�ʣ����ڴ��Ƿ��㹻
			
			//��ǰ�ڴ����û�����Է�����ڴ棬��Ҫ����
			if (_remaineBytes < sizeof(T))
			{
				_remaineBytes = 64 * 1024;
				//_memory = (char*)malloc(_remaineBytes);//���붨����64Kb�����ڴ�ռ�
				_memory = (char*)SystemAlloc(_remaineBytes >> PAGE_SHIFT);//���붨����64Kb�����ڴ�ռ�
				
				if (_memory == nullptr)
				{
					throw std::bad_alloc();
				}
			}
			//��֤һ�η���Ŀռ乻����µ�ǰƽ̨��ָ��
			size_t offset = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			obj = (T*)_memory;
			_memory += offset;
			_remaineBytes -= offset;
		}

		// ��λnew��ʽ����T���͹��캯��:���ڴ��ַobj������һ���µ�T���͵Ķ��󣬲����øö���Ĺ��캯����
		// ����ͨ��new�������ͬ���ǣ�������ʹ�ö�̬�ڴ�������������ڴ棬����ʹ��ָ�����ڴ��ַ��
		new(obj)T;
		return obj;
	}
	// �ض���Delete, �����ڴ�
	void Delete(T* obj)
	{
		// ��ʾ����obj�������������������ռ�
		obj->~T();
		// ��obj�ڴ��ͷ�嵽freeList
		NextObj(obj) = _freeList;
		_freeList = obj;
	}


private:
	char* _memory = nullptr;		// ָ������Ĵ���ڴ��ָ��
	void* _freeList = nullptr;		// ���������ͷָ�룬���浱ǰ���Ա��ظ����õĶ���
	size_t _remaineBytes = 0;		// ��ǰ�ڴ����ʣ���ڴ�ռ�
};
