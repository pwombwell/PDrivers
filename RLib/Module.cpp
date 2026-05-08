#include "swis.h"
#include "cmhg.h"

#include "Module.h"
#include "RLib/RLib.h"
#include "RLib/KernelWrapper.h"

// For debugLog
#include "OS.h"

extern "C" void __cpp_initialise(void);
extern "C" void __cpp_finalise(void);

Module* Module::s_instance = nullptr;

// Entry points from cmhg.
_kernel_oserror* Module_Initialisation(const char* tail,
                                       int podule_base,
                                       void* pw)
{
    __cpp_initialise();

    // There's no particular difference between allocating globals statically
    // or at runtime - both are RMA-allocated blocks, though static globals
    // have their ctors/dtors called either via newer versions of the Shared
    // C Library, or by __cpp_initialise()/__cpp_finalise.
    //
    // Having a Module::create() method that each PDriver must implement
    // allows the object to be owned by this file, which is possibly
    // less error-prone.
    //
    // There's also no reason not to have loads of globals (some static),
    // but it seems slightly more OO to have a single object that represents
    // the module, rather than objects scattered amongst source files as
    // you'd tend to do with a module written in C.
    MyError err = nullptr;
    Module* module = Module::create(pw);

    if (module == nullptr)
        err = MyError::OOM().toKernelOSError();

    if (err == nullptr) // note that earlier errors pass by this.
        err = module->initialise();

    if (err != nullptr)
        __cpp_finalise();

    return err.toKernelOSError();
}

_kernel_oserror* Module_Finalisation(int fatal, int podule_base, void* pw)
{
    MyError err;

    Module* module = Module::instance();
    // `module` really should be non-null here. Maybe error if not.

    // Give the module a chance to reject shutdown.
    if ((err = module->preFinalise()) != nullptr)
        return err.toKernelOSError();

    delete module;

    __cpp_finalise();

    return nullptr;
}

void Module_Service(int service, _kernel_swi_regs* r, void* pw)
{
    OS::KernelRegsWrapper regs(r);

    Module::instance()->handleServiceCall(service, regs);
}

MyError Module::claimVector(OS::Vector vector, void(*entry)(void))
{
    return _swix(OS_Claim, _INR(0,2), vector, entry, m_privateWord);
}

MyError Module::releaseVector(OS::Vector vector, void (*entry)(void))
{
    return _swix(OS_Release, _INR(0,2), vector, entry, m_privateWord);
}
