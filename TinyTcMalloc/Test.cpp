#define _CRT_SECURE_NO_WARNINGS 1
#include "MemoryPool.h"
#include "TinyTcMalloc.h"

void func1()
{
	for (size_t i = 0; i < 10; ++i)
	{
		TcMalloc(17);
	}
}

void func2()
{
	for (size_t i = 0; i < 20; ++i)
	{
		TcMalloc(5);
	}
}

// �����߳�
void TestThreads()
{
	std::thread t1(func1);
	std::thread t2(func2);


	t1.join();
	t2.join();
}

// ���Զ��뺯��
void TestSizeClass()
{
	/*cout << SizeClass::Index(1035) << endl;
	cout << SizeClass::Index(1025) << endl;
	cout << SizeClass::Index(1024) << endl;*/
	cout << SizeClass::Index(7) << endl;
	cout << SizeClass::Index(8) << endl;
	cout << SizeClass::Index(28) << endl;
}

//// �����ڴ�������ͷŽӿ�
//void TestMallocAndFree()
//{
//	void* ptr0 = TcMalloc(5);
//	void* ptr1 = TcMalloc(8);
//	void* ptr2 = TcMalloc(8);
//
//	TcFree(ptr0);
//	TcFree(ptr1);
//	TcFree(ptr2);
//}
//
//// ���Դ���ڴ������ͷ�
//void TestBigMemory()
//{
//	void* ptr1 = TcMalloc(65 * 1024);
//	TcFree(ptr1);
//
//	//Ҳ�п����������һ�����128ҳ���ڴ�
//	void* ptr2 = TcMalloc(129 * 4 * 1024);
//	TcFree(ptr2);
//}

int main()
{
	//TestObjectPool();
	//TestThreads();
	TestSizeClass();
	//TestMallocAndFree();
	//TestBigMemory();
	return 0;
}