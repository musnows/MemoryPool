#include "include/Span.hpp"
#include "include/FixedMemPool.hpp"
#include "include/ConcurrentAlloc.hpp"
using namespace mempool;

#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <ctime>
#include <thread>
using namespace std;

struct TreeNode
{
	int _val;
	TreeNode* _left;
	TreeNode* _right;

	TreeNode()
		:_val(0)
		, _left(nullptr)
		, _right(nullptr)
	{}
};
// 测试定长内存池
int TestFixedMemPool()
{
	const size_t Rounds = 5; // 申请释放的轮次
	const size_t N = 100000; // 每轮申请释放多少次

	std::vector<TreeNode*> v1;
	v1.reserve(N);
	// new的耗时测试
	size_t begin1 = clock();
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v1.push_back(new TreeNode);
		}
		for (int i = 0; i < N; ++i)
		{
			delete v1[i];
		}
		v1.clear();
	}
	size_t end1 = clock();

	std::vector<TreeNode*> v2;
	v2.reserve(N);
	// 定长内存池的耗时测试
	FixedMemoryPool<TreeNode> TNPool;
	size_t begin2 = clock();
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v2.push_back(TNPool.New());
		}
		for (int i = 0; i < N; ++i)
		{
			TNPool.Delete(v2[i]);
		}
		v2.clear();
	}
	size_t end2 = clock();

	cout << "new cost time:" << end1 - begin1 << endl;
	cout << "memory pool cost time:" << end2 - begin2 << endl;
	return 0;
}

void TestSpanListLock()
{
	SpanListLock stest;
	auto test_func = [&](size_t ms) {
		cout << this_thread::get_id() << " begin lock\n";
		stest.Lock();
		cout << this_thread::get_id() << " get lock\n";
		this_thread::sleep_for(std::chrono::microseconds(ms));
		stest.Unlock();
		cout << this_thread::get_id() << " unlock\n";
	};
	
	thread t1(test_func, 200);
	thread t2(test_func, 80);
	t1.detach();
	t2.join();

}

void TestNormalAlloc()
{
	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(8);
	void* p3 = ConcurrentAlloc(1);
	void* p4 = ConcurrentAlloc(7);
	void* p5 = ConcurrentAlloc(8);
	void* p6 = ConcurrentAlloc(8);
	void* p7 = ConcurrentAlloc(8);


	cout << p1 << endl;
	cout << p2 << endl;
	cout << p3 << endl;
	cout << p4 << endl;
	cout << p5 << endl;
	cout << p6 << endl;
	cout << p7 << endl;


	ConcurrentFree(p1);
	ConcurrentFree(p2);
	ConcurrentFree(p3);
	ConcurrentFree(p4);
	ConcurrentFree(p5);
	ConcurrentFree(p6);
	ConcurrentFree(p7);
}


void MultiThreadAlloc1()
{
	try{
		std::vector<void*> v;
		for (int i = 0; i < 50; i++)
		{
			void* ptr = ConcurrentAlloc(6);
			cout << "1 - " << std::this_thread::get_id() << " - " << ptr << endl;
			v.push_back(ptr);
		}

		for (auto e : v)
		{
			ConcurrentFree(e);
		}
	}
	catch (...)
	{
		cout << "error in MultiThreadAlloc1\n";
		return;
	}
}

void MultiThreadAlloc2()
{
	try {
		std::vector<void*> v;
		for (int i = 0; i < 50; i++)
		{
			void* ptr = ConcurrentAlloc(20);
			cout << "2 - " << std::this_thread::get_id() << " - " << ptr << endl;
			v.push_back(ptr);
		}

		for (auto e : v)
		{
			ConcurrentFree(e);
		}
	}
	catch (...)
	{
		cout << "error in MultiThreadAlloc2\n";
		return;
	}
}

// 测试多线程处理是否有问题
void TestMultiThread()
{
	std::thread t1(MultiThreadAlloc1);
	std::thread t2(MultiThreadAlloc2);

	t1.detach();
	t2.join();
}

int main()
{
	TestMultiThread();
	return 0;
}