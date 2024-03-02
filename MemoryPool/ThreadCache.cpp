#include "ThreadCache.h"
#include "CentralCache.h"

namespace mempool
{
	void* ThreadCache::Allocate(size_t bytes)
	{
		assert(bytes < MAX_SIZE);
		// 计算对应哈希表哪一个下标
		size_t index = SizeClass::Index(bytes);
		// 判断freelist中是否还有内存
		if (_freeList[index].Size() != 0)
		{
			// 有，直接分配
			return _freeList[index].Pop();
		}
		else
		{ 
			// 没有，去找中心缓存要
			return FetchFromCentralCache(index, SizeClass::RoundUp(bytes));
		}
	}

	void ThreadCache::Deallocate(void* ptr, size_t bytes) 
	{
		assert(ptr != nullptr);
		assert(bytes < MAX_SIZE);

		size_t index = SizeClass::Index(bytes);
		_freeList[index].Push(ptr); // 插入对应位置

		// 当前链表长度已经大于一次性向中心缓存申请的长度，代表链表中的内存大概率用不完
		if (_freeList[index].Size() >= _freeList[index].GetMaxSize())
		{
			ReleaseToCentralCache(_freeList[index], bytes);
		}
	}

	void* ThreadCache::FetchFromCentralCache(size_t index, size_t bytes)
	{
		// 注意，std和windows.h中都有一个min宏
		size_t batchNum = min(_freeList[index].GetMaxSize(), SizeClass::NumMoveSize(bytes));
		if (_freeList[index].GetMaxSize() == batchNum)
		{
			_freeList[index].IncreaseMaxSize(1); // 每申请一次，就将下次申请的阈值加一
			// 也可以修改这个值
		}

		void* start = nullptr;
		void* end = nullptr;
		size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, bytes);

		if (actualNum == 1)
		{
			assert(start == end);
			return start; // 只有一个的时候，直接返回
		}
		else
		{
			_freeList[index].PushRange(NextObj(start), end, actualNum - 1);
			return start;
		}
	}

	void ThreadCache::ReleaseToCentralCache(FreeList& list, size_t bytes)
	{
		void* start = nullptr;
		void* end = nullptr;
		list.PopRange(start, end, list.GetMaxSize()); // 将这个链表全部pop
		// 释放
		CentralCache::GetInstance()->ReleaseListToSpans(start,bytes);
	}
}	