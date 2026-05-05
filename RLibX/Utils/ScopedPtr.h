#pragma once

#include <stddef.h>

// Maybe this could evolve into the world's least featured unique_ptr<>.
template<class T>
class ScopedPtr {
public:
    ScopedPtr() : m_ptr(nullptr) { }
    ~ScopedPtr() {
        delete [] m_ptr;
    }

    bool allocate(uint32_t count) {
        reset(new T[count]);
        return m_ptr != nullptr;
    }

    void reset(T* p = nullptr) {
        if (m_ptr != p) {
            delete [] m_ptr;
            m_ptr = p;
        }
    }

    T* release() {
        T* p = m_ptr;
        m_ptr = nullptr;
        return p;
    }

    T* get() { return m_ptr; }
    const T* get() const { return m_ptr; }

    T& operator[](uint32_t i) { return m_ptr[i]; }
    const T& operator[](uint32_t i) const { return m_ptr[i]; }

private:
    ScopedPtr(const ScopedPtr&);
    ScopedPtr& operator=(const ScopedPtr&);

private:
    T* m_ptr;
};
