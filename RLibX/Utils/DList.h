#ifndef RLIBX_UTILS_DLIST_H
#define RLIBX_UTILS_DLIST_H

#include <stddef.h>

namespace riscos {
namespace Utils {

/*
 * Intrusive doubly-linked list.
 *
 * Usage:
 *
 *   struct Job {
 *       DListHook<Job> m_listHook;
 *   };
 *
 *   DList<Job> jobs;
 */

template <typename T>
class DListHook {
public:
    DListHook() : m_prev(0), m_next(0) { }

    ~DListHook() {
        if (linked())
            unlink();
    }

    bool linked() const { return m_prev != 0; }

    void unlink() {
        if (!linked())
            return;

        DListHook* prev = m_prev;
        DListHook* next = m_next;
        prev->m_next = next;
        next->m_prev = prev;
        m_prev = 0;
        m_next = 0;
    }

    DListHook* m_prev;
    DListHook* m_next;
};

template <typename T>
class DList {
public:
    DList()
        : m_root()
    {
        m_root.m_prev = &m_root;
        m_root.m_next = &m_root;
    }

    bool empty() const { return m_root.m_next == &m_root; }

    T& front() { return *value_from_hook(m_root.m_next); }
    const T& front() const { return *value_from_hook(m_root.m_next); }

    T& back() { return *value_from_hook(m_root.m_prev); }
    const T& back() const { return *value_from_hook(m_root.m_prev); }

    void push_front(T& value) {
        Hook& node = hook(value);
        if (node.linked())
            node.unlink();

        link_between(&node, &m_root, m_root.m_next);
    }

    void push_back(T& value) {
        Hook& node = hook(value);
        if (node.linked())
            node.unlink();

        link_between(&node, m_root.m_prev, &m_root);
    }

    void pop_front() {
        if (!empty())
            m_root.m_next->unlink();
    }

    void pop_back() {
        if (!empty())
            m_root.m_prev->unlink();
    }

    void unlink(T& value) {
        Hook& node = hook(value);
        if (node.linked())
            node.unlink();
    }

    void clear() {
        while (!empty())
            pop_front();
    }

    bool remove(T& value) {
        Hook& node = hook(value);
        if (!node.linked())
            return false;

        node.unlink();
        return true;
    }

    T* first() { return empty() ? 0 : value_from_hook(m_root.m_next); }
    const T* first() const { return empty() ? 0 : value_from_hook(m_root.m_next); }

    T* last() { return empty() ? 0 : value_from_hook(m_root.m_prev); }
    const T* last() const { return empty() ? 0 : value_from_hook(m_root.m_prev); }

    T* next(T* value) {
        Hook* node = hook(*value).m_next;
        return node == &m_root ? 0 : value_from_hook(node);
    }

    const T* next(const T* value) const {
        const Hook* node = hook(*value).m_next;
        return node == &m_root ? 0 : value_from_hook(node);
    }

    T* prev(T* value) {
        Hook* node = hook(*value).m_prev;
        return node == &m_root ? 0 : value_from_hook(node);
    }

    const T* prev(const T* value) const {
        const Hook* node = hook(*value).m_prev;
        return node == &m_root ? 0 : value_from_hook(node);
    }

    bool isOnAnyList(const T& value) const { return hook(value).linked(); }

private:
    typedef DListHook<T> Hook;

    static Hook& hook(T& value) { return value.m_listHook; }
    static const Hook& hook(const T& value) { return value.m_listHook; }

    static ptrdiff_t hook_offset() {
        const T* ptr = 0;
        return (const char*)(&(ptr->m_listHook)) - (const char*)ptr;
    }

    static T* value_from_hook(Hook* node) {
        return (T*)((char*)node - hook_offset());
    }

    static const T* value_from_hook(const Hook* node) {
        return (const T*)((const char*)node - hook_offset());
    }

    static void link_between(Hook* node, Hook* prev, Hook* next) {
        node->m_prev = prev;
        node->m_next = next;
        prev->m_next = node;
        next->m_prev = node;
    }

    Hook m_root;
};

} // namespace Utils
} // namespace riscos

#endif
