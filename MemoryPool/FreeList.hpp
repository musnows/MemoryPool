#pragma once
#include <cassert>
#include "utils.hpp"

namespace mempool {


// 管理切分好的小对象的自由链表
class FreeList
{
public:
	void Push(void* obj)
	{
		assert(obj);

		// 头插
		NextObj(obj) = _freeList;
		_freeList = obj;

		_size++;
	}

	void PushRange(void* start, void* end, size_t n)
	{
		NextObj(end) = _freeList;
		_freeList = start;

		_size += n;
	}

	void PopRange(void*& start, void*& end, size_t n)
	{
		assert(n <= _size);
		start = _freeList;
		end = start;

		for (size_t i = 0; i < n - 1; ++i)
		{
			end = NextObj(end);
		}

		_freeList = NextObj(end);
		NextObj(end) = nullptr;
		_size -= n;
	}

	void* Pop()
	{
		assert(_freeList);

		// 头删
		void* obj = _freeList;
		_freeList = NextObj(obj);
		_size--;

		return obj;
	}
	// 判断链表是否为空
	bool Empty()
	{
		return _freeList == nullptr;
	}
	// 获取MaxSize的值
	size_t GetMaxSize()
	{
		return _maxSize;
	}
	// 给MaxSize+=size
	void IncreaseMaxSize(size_t size)
	{
		_maxSize += size;
	}
	// 获取当前freelist的大小
	size_t Size()
	{
		return _size;
	}

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1; // ThreadCache申请内存时用于限制的阈值
	size_t _size = 0; // 链表长度
};

}