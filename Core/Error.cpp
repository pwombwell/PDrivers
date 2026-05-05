#include "PDriver.h"

#include "Constants.h"
#include "Job.h"
#include "MsgCode.h"
#include "OS.h"
#include "Workspace.h"

#include "RLib/OS/Error.h"
#include "RLibX/OSByte.h"

#include <string.h>

/* SWI/system constants. */
extern const uint32_t EscapeHandler;

/* Error blocks from assembly. */
DECLARE_INTERNAT_ERROR_BLOCK(Escape, "Escape");

static const char s_cancelSuffixToken[] = "CanSuff";

#if Medusa
// Buffer for error messages (persists beyond job deletion), yet its
// existence is still flagged per-job. Pre-Medusa this was on the Job.
// Possibly move back to Job, then copy to global on destruction?
//
// need to allocate a buffer for error messages
// that still exists once the job has been deleted
static OS::MaxErrorBlock s_errorBuffer; // globalerrorbuffer
#endif

// Subroutine to make a persistent error, normally adding a suffix.
MyError CoreJobWS::makePersistentError(MyError srcError)
{
    MyError err;

    // is there a pending persistent error? If yes, don't disturb it.
    if (m_persistentError)
        return srcError;

    MsgCode& msgCode = CoreWS::instance().messages;

    const char* suffix;
    uint32_t suffixLen;
    if ((err = msgCode.lookupToken(s_cancelSuffixToken, suffix, suffixLen)) != nullptr)
        return err;

    // We want srcError's string to be copied and terminated with the
    // suffix that's " (print cancelled)" (by default). It it doesn't fit,
    // truncate srcError's string with ellipses, and ensure the suffix is
    // appended.
    //
    // eg:
    //   "Some Error." should be come "Some Error. (print cancelled)"
    //   "Truncated Error." should be come "Truncated Er... (print cancelled)"

    // Keep room for the looked-up suffix and the final terminator.
    const uint32_t errMessSize = 252;
    if (suffixLen >= errMessSize)
        return makePersistentNoSuffix(srcError);

    const uint32_t maxPrefixChars = errMessSize - suffixLen - 1;

    // Temporary prefix buffer. ARM version claimed from the RMA.
    char temp[252];
    uint32_t copied = 0;
    const char* src = srcError.message();
    while (copied < maxPrefixChars && src[copied] != '\0') {
        temp[copied] = src[copied];
        ++copied;
    }

    // Copy srcError's string to temp buffer, terminating if we reach either
    // the end of destination buffer, or '\0'.

    // The ARM original is broken - its clearly intended to copy to a max
    // size, but r3 is never modified. It only checks for '\0' so can overrun
    // both src and dst buffers when adding the ellipsis.

    const bool truncated = src[copied] != '\0';
    if (truncated) {
        // The ARM version appends four dots, because that's what you do.
        if (maxPrefixChars >= 4) {
            copied = maxPrefixChars;
            temp[copied - 4] = '.';
            temp[copied - 3] = '.';
            temp[copied - 2] = '.';
            temp[copied - 1] = '.';
        }
    }

    temp[copied] = '\0';

    OS::MaxErrorBlock& dst = s_errorBuffer;

    MsgCode::ScopedOpen msgs(CoreWS::instance().messages);
    if ((err = msgs.open()) != nullptr)
        return err;

    dst.errnum = srcError.number();
    if ((err = (*msgs).lookup(s_cancelSuffixToken,
                              dst.errmess, sizeof(dst.errmess),
                              temp)) != nullptr)
        return err;

    m_persistentError = true;

    return dst;
}

MyError CoreJobWS::makePersistentError(OS::ErrorView token)
{
    MsgCode& msg(CoreWS::instance().messages);

    return makePersistentError(msg.lookupError(token));
}


MyError CoreJobWS::makePersistentNoSuffix(MyError err)
{
    if (!err)
        return nullptr;

    if (m_persistentError)
        return err;

    OS::MaxErrorBlock& errorBuffer = s_errorBuffer;

    errorBuffer.errnum = err.number();

    char* dst = errorBuffer.errmess;
    const char* src = err.message();
    uint32_t max_copy = sizeof(errorBuffer.errmess) - 1;
    uint32_t copied = 0;

    while (copied < max_copy && src[copied] != '\0') {
        dst[copied] = src[copied];
        copied++;
    }

    if (copied >= max_copy)
        dst[max_copy] = '\0';
    else
        dst[copied] = '\0';

    errorBuffer.errmess[sizeof(errorBuffer.errmess) - 1] = '\0';

    m_persistentError = true;

    return errorBuffer;
}

MyError CoreJobWS::checkPersistentError() const
{
    if (!m_persistentError)
        return nullptr;

    return s_errorBuffer;
}
