#pragma once

#include "Commons.h"
#include <map>
#include <mutex>

namespace memoryPool {

class PageCache
{
public:
  // 外界接口
  void* allocSpan(std::size_t numPages);
  void deallocSpan(void* addr, std::size_t numPages);
  void reset();

public:
  static PageCache& getInstance()
  {
    static PageCache instance;
    return instance;
  }

  PageCache(const PageCache&) = delete;
  PageCache& operator=(const PageCache&) = delete;

private:
  PageCache() = default;
  void* systemAlloc(std::size_t numPages);

  struct Span {
    void* pgAddr;
    std::size_t numPages;
    Span* next;
  };

  std::map<std::size_t, Span*> freeSpans_;  // 按页数分桶
  std::map<void*, Span*> spanMap_;          // 记录每个 span 的起始地址，方便进行"相邻页合并"
  std::mutex pglock_;                       // 互斥锁同步
};

} // namespace memoryPool
