#pragma once
#include "Span.hpp"
#include "Utils.hpp"
#include "FixedMemPool.hpp"

namespace mempool
{
	class PageCache
	{
	public:
		static PageCache* GetInstance()
		{
			static PageCache _sInstance;
			return &_sInstance;
		}

		// 通过内存地址获取它对应的span对象地址，自带加锁
		Span* MapObjectToSpan(void* obj);

		// 遍历设置映射关系
		void SetMapObjectToSpan(Span* span);

		// 释放空闲span回到Pagecache，并合并相邻的span
		void ReleaseSpanToPageCache(Span* span);

		// 获取一个K页的span
		Span* NewSpan(size_t k);

		inline void Lock(){
			_pageMtx.lock();
		}

		inline void Unlock(){
			_pageMtx.unlock();
		}

	private:
		SpanList _spanList[NUM_PAGES]; // 通过页面数量映射Span
		FixedMemoryPool<Span> _spanPool; // 获取Span对象的定长内存池
		// 用于映射页号和Span对象地址
		std::unordered_map<PageID, Span*> _idSpanMap;
		// PageCache采用全局锁
		std::mutex _pageMtx;

		// 单例模式，私有构造函数
		PageCache(){}
		PageCache(const PageCache&) = delete;
	};
}