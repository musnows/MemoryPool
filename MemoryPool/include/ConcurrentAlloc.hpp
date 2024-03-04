#pragma once
#include "Utils.hpp"
#include "PageCache.h"
#include "ThreadCache.h"

namespace mempool
{
	static void* ConcurrentAlloc(size_t size)
	{
		if (size > MAX_SIZE)
		{
			size_t alignSize = SizeClass::RoundUp(size);
			size_t kpage = alignSize >> PAGE_SHIFT;

			PageCache::GetInstance()->Lock();
			Span* span = PageCache::GetInstance()->NewSpan(kpage);
			span->_objSize = size; // 对于大块内存而言是没有拆分的，这里必须要设置一下大小
			PageCache::GetInstance()->Unlock();

			void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
			return ptr;
		}
		else
		{
			// 通过TLS 每个线程无锁的获取自己的专属的ThreadCache对象
			if (TLSThreadCache == nullptr)
			{
				static FixedMemoryPool<ThreadCache> tcPool; // 这里的static变量是全局可见的
				TLSThreadCache = tcPool.New();
			}

			return TLSThreadCache->Allocate(size);
		}
	}

	static void ConcurrentFree(void* ptr)
	{
		Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
		size_t size = span->_objSize;

		if (size > MAX_SIZE)
		{
			PageCache::GetInstance()->Lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->Unlock();
		}
		else
		{
			assert(TLSThreadCache);
			TLSThreadCache->Deallocate(ptr, size);
		}
	}
}