#include "Vector.h"

#include "swis.h"

MyError claimVector(OS::Vector vector, void(*entry)(void), void* pw)
{
    return _swix(OS_Claim, _INR(0,2), vector, entry, pw);
}

MyError releaseVector(OS::Vector vector, void (*entry)(void), void* pw)
{
    return _swix(OS_Release, _INR(0,2), vector, entry, pw);
}
