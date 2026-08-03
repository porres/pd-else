// Copyright 2014 Emilie Gillet.
//
// Stream buffer for serialization.

#ifndef STMLIB_UTILS_BUFFER_ALLOCATOR_H_
#define STMLIB_UTILS_BUFFER_ALLOCATOR_H_

#include "stmlib/stmlib.h"

namespace stmlib {

class BufferAllocator {
 public:
  BufferAllocator() { }
  ~BufferAllocator() { }
  
  BufferAllocator(void* buffer, size_t size) {
    Init(buffer, size);
  }
  
  inline void Init(void* buffer, size_t size) {
    buffer_ = static_cast<uint8_t*>(buffer);
    size_ = size;
    Free();
  }

  template<typename T>
  inline T* Allocate() {
    return Allocate<T>(1);
  }
  
  template<typename T>
  inline T* Allocate(size_t size) {
    size_t size_bytes = sizeof(T) * size;
    if (size_bytes <= free_) {
      T* start = static_cast<T*>(static_cast<void*>(next_));
      next_ += size_bytes;
      free_ -= size_bytes;
      return start;
    } else {
      return NULL;
    }
  }
  
  inline void Free() {
    next_ = buffer_;
    free_ = size_;
  }
  
  inline size_t free() const { return free_; }

 private:
  uint8_t* next_;
  uint8_t* buffer_;
  size_t free_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(BufferAllocator);
};

}  // namespace stmlib

#endif   // STMLIB_UTILS_STREAM_BUFFER_H_
