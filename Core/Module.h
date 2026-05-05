#ifndef CORE_MODULE_H
#define CORE_MODULE_H

#include "RLib/RLib.h"
#include "RLib/OS.h"

#include "RLib/OS/Vector.h"

class Module {
public:
    virtual ~Module() { s_instance = nullptr; }

    // All PDriver implementations must implement Module::create() and return
    // an instance of their subclass.
    static Module* create(void* pw);

    // Called from Module_Initialise after create().
    virtual MyError initialise() { return nullptr; }

    // Give the module a chance to reject finalisation.
    virtual MyError preFinalise() { return nullptr; }

    // The Module should always be the first object created, and last object
    // destroyed in the module initialise and finalise calls.
    //
    // Subclasses may choose to return a reference to themselves if they
    // implement their own instance(), but care must be taken for any global
    // destructors not to assume an instance of Module exists - they could
    // either check Module::instance() != nullptr, or instead of being
    // global, be members of any subclass.
    static Module* instance() { return s_instance; }

    // Allow subclasses to listen for any service calls registered via cmhg.
    virtual void handleServiceCall(uint32_t service, OS::Regs& regs) { }

    // Return the opaque private word, used for claiming vectors, etc.
    void* privateWord() const { return m_privateWord; }

    // Some utility functions that only Modules are likely to use.

    // `entry` is the entry point from cmhg, not a C/C++ function.
    MyError claimVector(OS::Vector vector, void(*entry)(void));
    MyError releaseVector(OS::Vector vector, void(*entry)(void));

protected:
    Module(void* pw) : m_privateWord(pw) { s_instance = this; }

private:
    static Module*  s_instance;     // Static instance of the subclass.
    void*           m_privateWord;
};

#endif
