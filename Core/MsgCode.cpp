#include "swis.h"

#include "PDriver.h"
#include "MsgCode.h"

#include "OS.h"
#include "Workspace.h"

#include "RLib/MessageTrans.h"
#include "RLib/Constants/Service.h"

#include <stdio.h>

MyError MsgCode::open()
{
    MyError err;

    if (m_shared) {
        m_shared->refCount++;
        return nullptr;
    }

    // Ask other PDrivers to give us m_shared, if they have one.
    uint32_t serviced;
    SharedMessages* shared = nullptr;
    if ((err = _swix(OS_ServiceCall, _IN(1)|_OUT(1)|_OUT(3),
                     Service_PDriverGetMessages, &serviced, &shared)) != nullptr)
        return err;

    if (serviced == Service_Serviced && shared) {
        m_shared = shared;
        m_shared->refCount++;
        return nullptr;
    }

    // Must use OS_Module to claim as other modules may free it.
    // malloc() doesn't always map directly to OS_Module Claim.
    void* block = nullptr;
    if ((err = rma_claim(sizeof(SharedMessages), block)) != nullptr)
        return err;

    shared = (SharedMessages*)block;
    shared->refCount = 0;

    if ((err = MessageTrans::xopenFile(*shared,
                                       "Resources:$.Resources.PDrivers.Messages")) != nullptr)
    {
        rma_free(shared);
        return err;
    }

    m_shared = shared;
    m_shared->refCount++;

    return nullptr;
}

void MsgCode::close()
{
    if (!m_shared || m_shared->refCount == 0)
        return;

    MessageTrans::xcloseFile(*m_shared);

    m_shared->refCount--;
    if (m_shared->refCount != 0)
        return;

    rma_free(m_shared);
    m_shared = nullptr;
}

MyError MsgCode::lookupError(OS::ErrorView token,
                             const char* param0, const char* param1,
                             const char* param2, const char* param3)
{
    MyError err;
    if ((err = open()) != nullptr)
        return err;

    // `err` could be a pointer to our workspaces's error buffer,
    // or the passed in 'error' if the lookup failed. Or, potentially,
    // a MessageTrans buffer if there're no shared messages.
    err = MessageTrans::xerrorLookup(token,
                                     *m_shared,
                                     buffer,
                                     param0, param1,
                                     param2, param3);

    close();

    return err;
}

MyError MsgCode::lookupToken(const char* token,
                             const char*& result, uint32_t& resultLen)
{
    MyError err;
    
    if ((err = open()) != nullptr)
        return err;

    err = MessageTrans::lookup(*m_shared, token, result, resultLen);

    close();

    return err;
}

MyError MsgCode::lookup(const char* token, char* buffer, uint32_t bufferLen,
                        const char* param0, const char* param1,
                        const char* param2, const char* param3)
{
    MyError err;
    
    if ((err = open()) != nullptr)
        return err;

    err = MessageTrans::lookup(*m_shared, token, buffer, bufferLen,
                               param0, param1, param2, param3);

    close();

    return err;
}

void MsgCode::serviceMessages(OS::Regs& regs)
{
    if (m_shared == nullptr)
        return;

    regs[1] = Service_Serviced;
    regs[3] = (int32_t)(uintptr_t)m_shared;
}
