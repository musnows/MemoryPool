#include "../include/PageCache.h"

namespace mempool
{
	// 通过内存地址获取它对应的span对象地址
	Span* PageCache::MapObjectToSpan(void* obj)
	{
		// 一页是8KB，在这个页内的所有地址/8KB计算出来的页号都一样！
		// 因为整除后余数被省略了
		PageID id = (reinterpret_cast<PageID>(obj) >> PAGE_SHIFT);
		// 查询的时候加锁
		std::unique_lock<std::mutex> lock(_pageMtx);
		auto ret = _idSpanMap.find(id);
		if(ret != _idSpanMap.end())
		{
			return ret->second;
		}
		else 
		{
			return nullptr;
		}
	}

	void PageCache::SetMapObjectToSpan(Span* span)
	{
		for (PageID i = span->_pageId; i < span->_pageId + span->_n; i++)
		{
			_idSpanMap[i] = span;
		}
	}


	// 释放空闲span回到Pagecache，并合并相邻的span
	void PageCache::ReleaseSpanToPageCache(Span* span)
	{
		assert(span != nullptr);

		// 大于128pages的无法处理，直接返回给堆
		if (span->_n >= NUM_PAGES)
		{
			SystemFree(reinterpret_cast<void*>(span->_pageId << PAGE_SHIFT));
			_spanPool.Delete(span); // 释放定长内存池里面的内存
			// 因为大于128Pages的并不是PageCache管理的，所以不需要操作idMap
			return;
		}

		// 向前合并
		while (true)
		{
			PageID prevId = span->_pageId - 1; // 前一个页的id
			// 如果存在，肯定是另外一个Span对象管理的
			auto ret = _idSpanMap.find(prevId);
			if (ret == _idSpanMap.end())
			{
				break; // 找不到，不合并
			}
			// 找到了
			Span* prev = ret->second;
			if (prev->_isUsed)
			{
				break; // 正在使用，不合并
			}
			//判断页面数量有没有超，超过128都没有办法管理
			if (prev->_n + span->_n >= NUM_PAGES)
			{
				break;
			}

			// 合并，使用span来合并，因为后续都是操作span
			span->_pageId = prev->_pageId;
			span->_n += prev->_n;

			_spanList[prev->_n].Erase(prev);
			_spanPool.Delete(prev);
		}

		// 向后合并
		while (true)
		{
			PageID nextId = span->_pageId - 1; // 后一个Span的id
			auto ret = _idSpanMap.find(nextId);
			if (ret == _idSpanMap.end())
			{
				// 找不到，不合并
				break;
			}
			Span* next = ret->second;
			if (next->_isUsed)
			{
				break; // 正在使用，不合并
			}
			//判断页面数量有没有超，超过128都没有办法管理
			if (next->_n + span->_n >= NUM_PAGES)
			{
				break;
			}
			// 合并
			span->_n += next->_n;

			_spanList[next->_n].Erase(next);
			_spanPool.Delete(next);
		}


		// 插入链表
		_spanList[span->_n].PushFront(span);
		span->_isUsed = false;

		// 需要重新设置idmap
		SetMapObjectToSpan(span);
	}

	// 获取一个K页的span
	Span* PageCache::NewSpan(size_t k)
	{
		// 超过管理范围，直接申请并设置span
		if (k >= NUM_PAGES)
		{
			void* ptr = SystemAlloc(k); // 这里直接传入K就可以了
			Span* span = _spanPool.New();
			span->_n = k;
			span->_pageId = reinterpret_cast<PageID>(ptr) >> PAGE_SHIFT;
			// 这里必须要设置，否则释放内存时无法确认是否大于256KB
			_idSpanMap[span->_pageId] = span; 
			return span;
		}

		// 判断list里面有没有合适的span
		if (!_spanList[k].Empty())
		{
			Span* span = _spanList[k].PopFront();
			SetMapObjectToSpan(span);
			return span;
		}
		// 没有，找大的进行拆分
		for (size_t i = k + 1; i < NUM_PAGES; i++)
		{
			// 跳过为空的
			if (_spanList[i].Empty())
			{
				continue;
			}
			// 找到了，进行拆分
			Span* bigSpan = _spanList[i].PopFront();
			Span* span = _spanPool.New();
			
			span->_n = k;
			span->_pageId = bigSpan->_pageId;

			bigSpan->_pageId += k; // 页号增加
			bigSpan->_n -= k; // 页面数量减少
			_spanList[bigSpan->_n].PushFront(bigSpan);
			
			SetMapObjectToSpan(span);
			return span;
		}
		// 还是没有，申请
		void* ptr = SystemAlloc(NUM_PAGES-1); // 按最大量128页进行申请
		Span* span = _spanPool.New();
		span->_n = (NUM_PAGES - 1);
		span->_pageId = reinterpret_cast<PageID>(ptr) >> PAGE_SHIFT;
		_spanList[span->_n].PushFront(span);

		//// 申请的内存不为128才会有剩余的span
		//if (k != (NUM_PAGES - 1))
		//{
		//	Span* leftSpan = _spanPool.New();
		//	leftSpan->_pageId = span->_pageId + k;
		//	leftSpan->_n = NUM_PAGES - k - 1;
		//	_spanList[leftSpan->_n].PushFront(leftSpan); // 插入
		//}
		//SetMapObjectToSpan(span);
		//return span;

		// 上面这种方式没有复用代码，我们可以直接将最大页插入链表，然后复用上述拆分部分的代码
		return NewSpan(k); // 因为已经申请了一个最大块，递归处理肯定能进行拆分
	}
}