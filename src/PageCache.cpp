#include "../include/PageCache.h"

namespace memoryPool {

void*
PageCache::allocSpan(std::size_t numPages)
{
  std::lock_guard<std::mutex> lock(pglock_);

  auto it = freeSpans_.lower_bound(numPages);

  if (it != freeSpans_.end()) {
    // 将找到的 span 从空闲链表中移除
    Span* span = it->second;
    if (!span->next)
      freeSpans_.erase(it);
    else
      freeSpans_[span->numPages] = span->next;
    span->next = nullptr;

    // 如果找到的页数大于请求的页数，进行分割
    if (span->numPages > numPages) {
      char* newAddr = static_cast<char*>(span->pgAddr) + numPages*PGSIZE;
      Span* newSpan = new Span {
        newAddr,
        span->numPages-numPages,
        nullptr,
      };

      auto& list = freeSpans_[newSpan->numPages];
      newSpan->next = list;
      list = newSpan;
      spanMap_[newAddr] = newSpan;

      span->numPages = numPages;
    }

    return span->pgAddr;
  }

  void* mem = systemAlloc(numPages);
  Span* newSpan = new Span {
    mem,
    numPages,
    nullptr,
  };
  spanMap_[mem] = newSpan;

  return mem;
}

void
PageCache::deallocSpan(void* addr, std::size_t numPages)
{
  std::lock_guard<std::mutex> lock(pglock_);

  auto it = spanMap_.find(addr);
  if (it == spanMap_.end())
    return;
  Span* span = it->second;

  void* nextAddr = static_cast<char*>(addr) + numPages*PGSIZE;
  auto nextIt = spanMap_.find(nextAddr);
  if (nextIt != spanMap_.end()) {
    Span* nextSpan = nextIt->second;
    auto it2 = freeSpans_.find(nextSpan->numPages);
    if (it2 != freeSpans_.end()) {
      auto& nextList = freeSpans_[nextSpan->numPages];
      Span* prev = nextList;
      bool no_free = false;
      if (prev == nextSpan) {
        nextList = nextList->next;
      } else {
        for (; prev->next && prev->next != nextSpan; prev = prev->next);
        if (prev->next == nextSpan)
          prev->next = nextSpan->next;
        else
          no_free = true;
      }
      if (!no_free) {
        nextSpan->next = nullptr;
        span->numPages += nextSpan->numPages;
        spanMap_.erase(nextIt);
        delete nextSpan;
      }
    }
  }

  auto& list = freeSpans_[span->numPages];
  span->next = list;
  list = span;
}

void*
PageCache::systemAlloc(std::size_t numPages)
{
  std::size_t bytes = numPages*PGSIZE;
  void* mem = mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED)
    return nullptr;
  return mem;
}

void
PageCache::reset()
{
  std::lock_guard<std::mutex> lock(pglock_);

  for (auto it = freeSpans_.begin(); it != freeSpans_.end();) {
    for (Span* span = it->second; span;) {
      Span* prev = span;
      span = span->next;
      spanMap_.erase(prev->pgAddr);
      munmap(prev->pgAddr, prev->numPages*PGSIZE);
      delete prev;
    }
    it = freeSpans_.erase(it);
  }
}

} // namespace memoryPool
