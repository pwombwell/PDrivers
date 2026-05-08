#ifndef RLIB_UTILS_VECTOR_H
#define RLIB_UTILS_VECTOR_H

#include <stdint.h>
#ifdef __CC_NORCROFT
#include "Norcroft/new.h"
#else
#include <new>
#endif

namespace RLib {

// WARNING: Only suitable for trivial types as members' dtors are not called.
// Norcroft doesn't seem to like templated placement delete.
template<class T>
class Vector {
public:
    Vector()
        : m_data(nullptr)
        , m_size(0)
        , m_capacity(0)
    {}

    Vector(const Vector&) = delete;
    Vector& operator=(const Vector&) = delete;

    ~Vector() {
        clear();
        delete[] reinterpret_cast<char*>(m_data);
        m_data = nullptr;
        m_capacity = 0;
    }

    bool reserve(uint32_t capacity) {
        if (capacity <= m_capacity)
            return true;

        T* newData = allocate(capacity);
        if (newData == nullptr)
            return false;

        for (uint32_t i = 0; i < m_size; ++i) {
            new (&newData[i]) T(m_data[i]);
//            m_data[i].~T(); // FIXME: Norcroft no likey.
        }

        delete[] reinterpret_cast<char*>(m_data);
        m_data = newData;
        m_capacity = capacity;
        return true;
    }

    bool append(const T& value) {
        if (m_size == m_capacity) {
            uint32_t newCapacity = (m_capacity == 0) ? 8 : m_capacity * 2;
            if (newCapacity <= m_capacity)
                return false;
            if (!reserve(newCapacity))
                return false;
        }

        new (&m_data[m_size]) T(value);
        ++m_size;
        return true;
    }

    void clear() {
//        for (uint32_t i = 0; i < m_size; ++i)
//            m_data[i].T::~T();
        m_size = 0;
    }

    uint32_t size() const { return m_size; }
    uint32_t capacity() const { return m_capacity; }
    bool empty() const { return m_size == 0; }

    T& operator[](uint32_t index) { return m_data[index]; }
    const T& operator[](uint32_t index) const { return m_data[index]; }

    T* data() { return m_data; }
    const T* data() const { return m_data; }

private:
    static T* allocate(uint32_t capacity) {
        if (capacity == 0)
            return nullptr;

        // Allocate as a POD to avoid ctors...
        char* raw = new char[sizeof(T) * capacity];
        return reinterpret_cast<T*>(raw);
    }

    T*       m_data;
    uint32_t m_size;
    uint32_t m_capacity;
};

} // namespace RLib

#endif
