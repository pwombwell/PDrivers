#pragma once


namespace riscos {
namespace Utils {

/*
 * Simple intrusive singly-linked list.
 *
 * Usage:
 *
 *   struct Job {
 *       Job* m_next;
 *
 *       Job* copy() const;
 *   };
 *
 *   riscos::Utils::List<Job> jobs;
 *   jobs.init();
 *   jobs.addHead(&job);
 *
 * Notes:
 * - T must contain an `m_next` pointer.
 * - `copy()` uses `T::copy()` to duplicate nodes.
 */
template <typename T>
class List {
public:
    List() : m_head(nullptr) {}

    T* head() const { return m_head; }
    bool isEmpty() const { return m_head == nullptr; }

    void addHead(T* node) {
        if (node == nullptr)
            return;

        node->m_next = m_head;
        m_head = node;
    }

    void addTail(T* node) {
        if (node == nullptr)
            return;

        node->m_next = nullptr;

        if (m_head == nullptr) {
            m_head = node;
            return;
        }

        T* tail = m_head;
        while (tail->m_next != nullptr)
            tail = tail->m_next;

        tail->m_next = node;
    }

    // returns true if found, otherwise false.
    bool remove(T* target) {
        T** link = &m_head;
        T* node = m_head;
        while (node != nullptr) {
            if (node == target) {
                *link = node->m_next;
                node->m_next = nullptr;
                return true;
            }

            link = &node->m_next;
            node = node->m_next;
        }

        return false;
    }

    T* detachHead() {
        T* node = m_head;
        if (node != nullptr) {
            m_head = node->m_next;
            node->m_next = nullptr;
        }
        return node;
    }

private:
    T* m_head;
};

} } // namespace riscos::Utils
