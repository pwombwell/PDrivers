#include "kernel.h"

#include "PDumper.h"

#include "swis.h"

MyError PDumper::lookupError(const OS::ErrorView& errBlock,
                             const char* param0,
                             const char* param1,
                             const char* param2,
                             const char* param3)
{
    MyError translatedError = nullptr;
    MyError err = _swix(PDumper_LookupError, _INR(0,4) | _OUT(0),
                        errBlock.data(), param0, param1, param2, param3,
                        &translatedError);
    if (err != nullptr)
        return err;

    return translatedError;
}
