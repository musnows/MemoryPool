#pragma once
#include "Utils.hpp"
#include "Span.hpp"

namespace mempool
{
	// 中心缓存采用单例模式设计
	class CentralCache
	{
	public:
		static CentralCache* GetInstance()
		{
			// C++11后static变量的初始化是线程安全的
			static CentralCache _sInstance;
			return &_sInstance;
		}

		// 获取一个非空的span
		Span* GetOneSpan(SpanListLock& list, size_t bytes);

		// 从中心缓存获取一定数量的对象给ThreadCache
		// start/end是链表指针的输出型参数
		// 返回值是最终给了多少个bytes大小的内存
		size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t bytes);

		// 回收ThreadCache中的list
		void ReleaseListToSpans(void* start, size_t bytes);
	private:
		SpanListLock _spanList[NUM_FREELIST];

		// 默认构造函数和拷贝构造函数都私有
		CentralCache(){}
		CentralCache(const CentralCache&) = delete;
		CentralCache& operator=(const CentralCache&) = delete;
	};
}