#ifndef RLIB_OSMODULE_H
#define RLIB_OSMODULE_H

#include "RLib/RLib.h"

class OSModule {
public:

enum Reason {
    Reason_Run    = 0,
    Reason_Load   = 1,
    Reason_Enter  = 2,
    Reason_Reinit = 3,
    Reason_Kill   = 4,
    Reason_Info   = 5,
    Reason_Alloc  = 6,
    Reason_Free   = 7
};

static MyError xmodule(uint32_t reason,
                      uint32_t r1,
                      void* r2,
                      uint32_t r3,
                      void** r2Out);
static MyError xalloc(int size, void*& blk);
static MyError xfree(void* blk);

};

#endif
