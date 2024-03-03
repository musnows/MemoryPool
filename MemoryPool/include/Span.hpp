#pragma once
#include "Utils.hpp"

namespace mempool
{

// 因为64位和32位中内存地址的长度不一样，所以对应类型也得不一样
#ifdef _WIN64 // 64位的win才会有这个宏
	typedef unsigned long long PageID;
#elif _WIN32 // 32和64位的win都会有这个宏
	typedef size_t PageID;
// 后续还需要插入一些Linux的宏
#elif __linux__
#if __WORDSIZE == 64 // 64位的Linux
	typedef unsigned long long PageID;
#else				 // 32位的Linux
	typedef size_t PageID;
#endif
#endif

	// PageCache和CentralCache中用于托管内存的类
	struct Span
	{
		PageID _pageId; // 页号
		size_t _n;		// 包含的页的数量

		Span *_next = nullptr; // 下一个Span（链表）
		Span *_prev = nullptr; // 上一个Span（链表）

		void *_list = nullptr; // 链接span拆分的小块内存
		size_t _useCount = 0;  // 使用数量，为0代表没有被使用

		size_t _objSize; // 拆分的小块内存的大小

		bool _isUsed = false; // 是否在占用
		// 当Span被分配给CentralCache后设置为true
	};

	// 带头双向循环链表
	class SpanList
	{
	public:
		// 初始化头节点
		SpanList()
		{
			_head = new Span;
			_head->_next = _head;
			_head->_prev = _head;
		}
		// 返回链表实际的头节点
		Span *Begin()
		{
			return _head->_next;
		}
		// 返回链表末尾
		Span *End()
		{
			return _head;
		}
		// 判断链表是否为空
		bool Empty()
		{
			return _head->_next == _head;
		}

		void PushFront(Span *span)
		{
			Insert(Begin(), span);
		}

		Span *PopFront()
		{
			Span *front = _head->_next;
			Erase(front);
			return front;
		}
		// 在pos位置插入newSpan
		void Insert(Span *pos, Span *newSpan)
		{
			assert(pos);
			assert(newSpan);

			Span *prev = pos->_prev;

			prev->_next = newSpan;
			newSpan->_prev = prev;
			newSpan->_next = pos;
			pos->_prev = newSpan;
		}
		// 删除pos
		void Erase(Span *pos)
		{
			assert(pos);
			assert(pos != _head); // 不可以删除头节点

			Span *prev = pos->_prev;
			Span *next = pos->_next;

			prev->_next = next;
			next->_prev = prev;
		}

	protected:		 // 因为需要继承所以用保护
		Span *_head; // 链表头节点
	};

	// 继承父类但包含桶锁，因为只有CentralCache需要桶锁
	class SpanListLock : public SpanList
	{
	public:
		void Lock()
		{
			_mtx.lock();
		}
		void Unlock()
		{
			_mtx.unlock();
		}

		bool TryLock()
		{
			return _mtx.try_lock();
		}

	private:
		std::mutex _mtx; // 桶锁
	};

}