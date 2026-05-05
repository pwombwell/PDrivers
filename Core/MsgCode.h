#ifndef CORE_MSGCODE_H
#define CORE_MSGCODE_H

#include "kernel.h"

#include "RLib/RLib.h"
#include "RLib/OS.h"
#include "RLib/OS/Error.h"
#include "RLib/MessageTrans.h"

// Internationalisation helpers (mirrors Core/MsgCode.s).
class MsgCode {
public:
    MsgCode() : m_shared(nullptr) { }

    MyError open();
    void close();

    template<size_t N>
    MyError lookupError(const OS::ErrorBlock<N>& error,
                        const char* param0 = nullptr,
                        const char* param1 = nullptr,
                        const char* param2 = nullptr,
                        const char* param3 = nullptr)
    {
        return lookupError(OS::ErrorView(error),
                           param0, param1, param2, param3);
    }

    MyError lookupError(OS::ErrorView error,
                        const char* param0 = nullptr,
                        const char* param1 = nullptr,
                        const char* param2 = nullptr,
                        const char* param3 = nullptr);

    // Lookup a token string.
    MyError lookupToken(const char* token,
                        const char*& result, uint32_t& resultLen);

    MyError lookup(const char* token,
                   char* buffer, uint32_t bufferLen,
                   const char* param0 = nullptr,
                   const char* param1 = nullptr,
                   const char* param2 = nullptr,
                   const char* param3 = nullptr);

    // Entry point for Service_PDriverGetMessages.
    void serviceMessages(OS::Regs& regs);

    // Helper to auto-close messagetrans.
    class ScopedOpen {
    public:
        ScopedOpen(MsgCode& msgs) : m_msgs(msgs), m_open(false) { }
        ~ScopedOpen() {
            if (m_open)
                m_msgs.close();
        }

        MsgCode& operator*() const { return m_msgs; }

        MyError open() {
            MyError err = m_msgs.open();
            if (err)
                return err;
            
            m_open = true;

            return nullptr;
        }

        // Optional force-early-close.
        void close() {
            if (!m_open)
                return;

            m_msgs.close();
            m_open = false;
        }

    private:
        MsgCode& m_msgs;
        bool     m_open;
    };

private:
    MsgCode(const MsgCode& rhs) {}

private:
#if 0
    // MessageTrans workspace (4 words + bool to flag if open - in BlockState)
    // Only used if PrivMesssages <> "" (or, in C++, #ifndef). However, this
    // code path is nonsense in the original ARM code - it corrupts, and
    // uses incorrect, registers.
    MessageTrans::BlockState m_private;
#endif

    // SharedMessages is a reference-counted MessageTrans block allocated in
    // the RMA that's literally shared between all PDrivers.
    struct SharedMessages : MessageTrans::Block {
        uint32_t refCount;
    };

    SharedMessages* m_shared;

    // Error buffer for our simple MessageTrans lookups.
    OS::MaxErrorBlock buffer;
};

#endif
