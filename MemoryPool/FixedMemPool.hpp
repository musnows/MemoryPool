#pragma once
// 定长内存池
#include "Utils.hpp"
#include <cstdlib>

namespace mempool {

template<class T>
class FixedMemoryPool
{
public:
	T* New()
	{
		T* obj = nullptr;

		// 优先把还回来内存块对象，再次重复利用
		if (_freeList)
		{
			void* next = NextObj(_freeList);
			obj = static_cast<T*>(_freeList);
			_freeList = next;
		}
		else
		{
			// 剩余内存不够一个对象大小时，则重新开大块空间
			if (_remainBytes < sizeof(T))
			{
				_remainBytes = 128 * 1024;
				_memory = static_cast<char*>(SystemAlloc(_remainBytes >> PAGE_SHIFT)); // 申请的是页面数量
			}

			obj = reinterpret_cast<T*>(_memory);
			assert(obj != nullptr);

			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize; // 加上对象大小后，会跳转到下一个内存块的位置
			_remainBytes -= objSize; // 减去剩余空间大小
		}

		// 定位new，显示调用T的构造函数初始化
		new(obj)T; // 即便是POD类型也是有个构造函数的

		return obj;
	}

	void Delete(T* obj)
	{
		// 显示调用析构函数
		obj->~T();

		// 头插，使用void**强转能保证转换后的空间一定是个指针的大小
		(*reinterpret_cast<void**>(obj)) = _freeList; // 指向原本的freelist开头的地址
		_freeList = obj; // 更新头节点
	}

private:
	char* _memory = nullptr; // 指向大块内存的指针
	size_t _remainBytes = 0; // 大块内存在切分过程中剩余字节数

	void* _freeList = nullptr; // 还回来过程中链接的自由链表的头指针
};
}