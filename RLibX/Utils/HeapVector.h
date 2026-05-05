#ifndef RLIBX_UTILS_HEAPVECTOR_H
#define RLIBX_UTILS_HEAPVECTOR_H

#include "std/array.h"

// Heap overflow vector.
//
// An array of size 'StackEntries' is allocated on the stack - if the
// runtime-chosen size is less than or equal to that, that
// array is used. Otherwise it is allocated on the heap.
//
// You should check data() != nullptr.
template<typename T, size_t StackEntries = 32>
class InlineBuffer {
public:
    InlineBuffer() : heap_(nullptr), data_(stack_.data()), size_(0) {}

    explicit InlineBuffer(size_t count)
        : heap_(nullptr), data_(stack_.data()), size_(count)
    {
        if (count > StackEntries) {
            data_ = heap_ = (T*)malloc(count * sizeof(T));

            if (heap_ == nullptr)
                size_ = 0;
        }
    }

    ~InlineBuffer()
    {
        free(heap_);
    }

    T* data() { return data_; }
    const T* data() const { return data_; }
    size_t size() const { return size_; }

    T& operator[](size_t i) { return data_[i]; }
    const T& operator[](size_t i) const { return data_[i]; }

    bool valid() const { return data_ != nullptr; }
    bool operator!() const { return data_ == nullptr; }

private:
    T* heap_;
    T* data_;
    size_t size_;
    std::array<T, StackEntries> stack_;
};

#endif
