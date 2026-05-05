#ifndef CORE_WRCHSTATE_H
#define CORE_WRCHSTATE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

class WrchState
{
public:
    WrchState() : m_queuePos(0), m_initChar(0), m_textBufferPos(0) { }

    void reset() {
        m_queuePos = 0;
        m_initChar = 0;
        m_textBufferPos = 0;
    }

    bool queueActive() const {
        return m_queuePos != 0;
    }

    uint8_t queuePos() const {
        return m_queuePos;
    }

    void setQueuePos(uint8_t pos) {
        m_queuePos = pos;
    }

    uint8_t initialChar() const {
        return m_initChar;
    }

    void beginSequence(uint8_t initial, uint8_t length) {
        m_initChar = initial;
        m_queuePos = length;
    }

    uint8_t appendQueued(uint8_t ch) {
        uint8_t* end = queueEnd();
        end[-m_queuePos] = ch;
        m_queuePos -= 1;
        return m_queuePos;
    }

    uint8_t textBufferPos() const {
        return m_textBufferPos;
    }

    void setTextBufferPos(uint8_t pos) {
        m_textBufferPos = pos;
    }

    uint8_t* queueEnd() {
        return m_queue + sizeof(m_queue);
    }

    const uint8_t* queueEnd() const {
        return m_queue + sizeof(m_queue);
    }

    uint32_t queueWord(size_t offsetFromEnd) const {
        uint32_t word = 0;
        memcpy(&word, queueEnd() - offsetFromEnd, sizeof(word));
        return word;
    }

private:
    uint8_t m_queuePos;
    uint8_t m_initChar;
    uint8_t m_textBufferPos;
    uint8_t m_queue[9];
};

#endif
