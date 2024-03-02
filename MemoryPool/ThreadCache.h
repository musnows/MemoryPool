#include "FreeList.hpp"
#include "Utils.hpp"

namespace mempool {

	class ThreadCache
	{
	public:
		// 申请和释放内存对象
		void* Allocate(size_t bytes);
		void Deallocate(void* ptr, size_t bytes);

		// 从中心缓存获取对象
		void* FetchFromCentralCache(size_t index, size_t bytes);

		// 释放对象时，链表过长时，回收内存回到中心缓存
		void ReleaseToCentralCache(FreeList& list, size_t bytes);
	private:
		FreeList _freeList[NUM_FREELIST];
	};

	// 线程局部变量，当检测到ThreadCache为空指针的时候进行初始化，每个线程都有自己的ThreadCache
	static _declspec(thread) ThreadCache* TLSThreadCache = nullptr;
}