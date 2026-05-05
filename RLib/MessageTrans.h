#ifndef RLIB_MESSAGETRANS_H
#define RLIB_MESSAGETRANS_H

#include "stdint.h"

namespace riscos {
namespace MessageTrans {

struct Block {
    uint32_t block[4];
};

} } // namespace riscos::MessageTrans

#endif
