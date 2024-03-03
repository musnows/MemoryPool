#include "../include/CentralCache.h"
#include "../include/PageCache.h"

namespace mempool {
	// 获取一个非空的span
	Span* CentralCache::GetOneSpan(SpanListLock& list, size_t bytes)
	{
		// 查找当前哈希桶下标位置中是否还有没有使用完毕内存的Span对象
		Span* itr = list.Begin();
		while (itr != list.End())
		{
			if (itr->_list != nullptr)
			{
				return itr;
			}
			else
			{
				itr = itr->_next;
			}
		}
		// 没有的时候需要向PageCache申请
		list.Unlock(); // 先解锁桶锁

		PageCache::GetInstance()->Lock();
		// 计算一次需要申请几个page的span
		Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(bytes));
		span->_isUsed = true;
		span->_objSize = bytes;
		PageCache::GetInstance()->Unlock();
		
		// 获取到Span了之后，由CentralCache负责拆分内存并链接在Span的list上
		// 这个时候还不需要解锁桶锁，因为此时这个Span只有当前线程知道
		char* start = reinterpret_cast<char*>(span->_pageId << PAGE_SHIFT); // 使用char*方便指针相加
		size_t spanSize = span->_n << PAGE_SHIFT; // 这个span托管的内存的大小
		char* end = start + spanSize;

		// 开始链接
		span->_list = start;
		start += bytes; // 先从下一个内存的位置开始
		void* cur = span->_list;
		while (start < end)
		{
			NextObj(cur) = start;
			cur = NextObj(cur);
			start += bytes;
		}
		NextObj(cur) = nullptr; // 结束链接

		// 将Span插入哈希桶，然后返回
		list.Lock();
		list.PushFront(span);

		return span;

	}

	// 从中心缓存获取一定数量的对象给ThreadCache
	size_t  CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t bytes)
	{
		size_t index = SizeClass::Index(bytes);
		_spanList[index].Lock();
		// 获取一个span对象
		Span* span = GetOneSpan(_spanList[index], bytes);

		// 从span中获取batchNum个对象，如果不够则能给多少给多少
		start = span->_list;
		end = start;
		size_t i = 0;
		size_t actualNum = 1; // 实际上给了多少个bytes大小的对象
		while (i < (batchNum - 1) && NextObj(end) != nullptr)
		{
			end = NextObj(end);
			i++;
			actualNum++;
		}
		span->_list = NextObj(end);
		NextObj(end) = nullptr;
		span->_useCount += actualNum;

		_spanList[index].Unlock();

		return actualNum;
	}

	// 回收ThreadCache中的list
	void  CentralCache::ReleaseListToSpans(void* start, size_t bytes)
	{
		size_t index = SizeClass::Index(bytes); // 计算下标位置
		_spanList[index].Lock();
		// 需要依次收回每个链的内存，且它们可能不在同一个span中
		while (start != nullptr)
		{
			void* next = NextObj(start);

			// 得到当前内存对应的span，并将其链接回去
			Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
			NextObj(start) = span->_list;
			span->_list = start;
			span->_useCount--;

			// 如果use count为0代表这个span中的所有内存都被回收了
			// centralCache可以将其释放给PageCache
			if (span->_useCount == 0)
			{
				// 在CentralCache的缓存中删除对应span
				_spanList[index].Erase(span);
				span->_list = nullptr;
				span->_next = nullptr;
				span->_prev = nullptr;

				// 因为需要访问pagecahce了，所以需要先接触桶锁
				_spanList[index].Unlock();

				PageCache::GetInstance()->Lock();
				PageCache::GetInstance()->ReleaseSpanToPageCache(span);
				PageCache::GetInstance()->Unlock();

				_spanList[index].Lock();
			}

			start = next;
		}

		_spanList[index].Unlock();
	}
}