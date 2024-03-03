#pragma once
// 这个头文件保存一些大家都能用得到的代码和头文件

#include <ctime>
#include <cassert>

#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>

#ifdef _WIN32
#include <Windows.h>
#elif __linux__ // linux
#include <sys/mman.h>
#endif // _WIN32

namespace mempool
{

	static const size_t PAGE_SHIFT = 13;	   // 内存地址对应一个页面的偏移量，13代表8KB
	static const size_t MAX_SIZE = 256 * 1024; // threadcache负责256kb
	static const size_t NUM_FREELIST = 208;	   // threadcache中freelist的长度
	static const size_t NUM_PAGES = 129;	   // PageCache最大管理128Page，使用129这样避免下标-1
#ifdef __linux__
	// Linux下需要一个map来存放地址和长度的关系，否则没有办法释放内存
	static std::unordered_map<void *, unsigned long long> allocPtrToBytes;
#endif

	// 直接用操作系统接口申请空间，参数为页面数量，一页8KB
	static void *SystemAlloc(size_t numOfPages)
	{
		unsigned long long bytes = numOfPages << PAGE_SHIFT;
#ifdef _WIN32
		void *ptr = VirtualAlloc(0, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#elif __linux__
		// linux下brk或者mmap
		void *ptr = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		allocPtrToBytes.emplace(ptr, bytes);
#else
		void *ptr = nullptr; // 不支持的操作系统
#endif
		if (ptr == nullptr)
		{
			throw std::bad_alloc();
		}

		return ptr;
	}

	// 直接用操作系统接口释放空间
	static void SystemFree(void *ptr)
	{
#ifdef _WIN32
		VirtualFree(ptr, 0, MEM_RELEASE);
#elif __linux__
		// linux
		auto ret = allocPtrToBytes.find(ptr);
		// 被释放的内存一定是以申请时的指针为起始地址的
		assert(ret == allocPtrToBytes.end()); // 没有找到，说明代表有问题
		// 被释放的长度
		unsigned long long bytes = ret->second;
		munmap(ptr, bytes);
#endif
	}

	// 因为自由链表是直接用内存中前4/8位来存放下一个位置的指针的
	// 所以只需要通过强转返回内存的前4/8位的地址就可以了
	static void *&NextObj(void *obj)
	{
		return *(static_cast<void **>(obj));
	}

	// 计算对象大小的对齐映射规则
	class SizeClass
	{
	public:
		// 计算需要申请内存的大小 可读性更强的版本
		/*size_t _RoundUp(size_t size, size_t alignNum)
		{
			size_t alignSize;
			if (size % alignNum != 0)
			{
				alignSize = (size / alignNum + 1)*alignNum;
			}
			else
			{
				alignSize = size;
			}

			return alignSize;
		}*/

		// 计算需要申请内存的大小
		static inline size_t _RoundUp(size_t bytes, size_t alignNum)
		{
			return ((bytes + alignNum - 1) & ~(alignNum - 1));
		}

		static size_t RoundUp(size_t bytes)
		{
			if (bytes <= 128)
			{
				return _RoundUp(bytes, 8);
			}
			else if (bytes <= 1024)
			{
				return _RoundUp(bytes, 16);
			}
			else if (bytes <= 8 * 1024)
			{
				return _RoundUp(bytes, 128);
			}
			else if (bytes <= 64 * 1024)
			{
				return _RoundUp(bytes, 1024);
			}
			else if (bytes <= 256 * 1024)
			{
				return _RoundUp(bytes, 8 * 1024);
			}
			else
			{ // 这里虽然左移13和8KB是一样的，但如果后续修改了PAGE_SHIFT就会不同
				return _RoundUp(bytes, 1 << PAGE_SHIFT);
			}
		}

		static inline size_t _Index(size_t bytes, size_t align_shift)
		{
			return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
		}

		// 计算映射的哪一个自由链表桶（ThreadCache）
		static size_t Index(size_t bytes)
		{
			assert(bytes <= MAX_SIZE);

			// 每个区间有多少个链
			static int group_array[4] = {16, 56, 56, 56};
			if (bytes <= 128)
			{
				return _Index(bytes, 3);
			}
			else if (bytes <= 1024)
			{
				return _Index(bytes - 128, 4) + group_array[0];
			}
			else if (bytes <= 8 * 1024)
			{
				return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
			}
			else if (bytes <= 64 * 1024)
			{
				return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
			}
			else if (bytes <= 256 * 1024)
			{
				return _Index(bytes - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
			}
			else
			{
				// 超出MAX_SIZE了
				assert(false);
			}

			return -1;
		}

		// ThreadCache一次获取多少个size大小的内存
		static size_t NumMoveSize(size_t bytes)
		{
			assert(bytes > 0);
			// 这里定义阈值区间为[2,512]
			// size大的时候，一次申请的数量少
			// size小的时候，一次申请的数量多
			int num = MAX_SIZE / bytes;
			// 修正区间
			if (num < 2)
			{
				num = 2;
			}
			else if (num > 512)
			{
				num = 512;
			}

			return num;
		}

		// 计算一次向系统获取几个页
		static size_t NumMovePage(size_t bytes)
		{
			size_t num = NumMoveSize(bytes);
			size_t num_page = num * bytes;

			num_page >>= PAGE_SHIFT;
			if (num_page == 0)
			{
				num_page = 1; // 至少会分配一页的空间
			}

			return num_page;
		}
	};

}