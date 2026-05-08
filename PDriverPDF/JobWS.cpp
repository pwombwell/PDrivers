#include "JobWS.h"

JobWS::~JobWS()
{
    // This has drifted from PDriverPS. Make them the same again.
    DeclaredFont* font;
    while ((font = declaredfonts.detachHead()) != nullptr)
        delete font;
}
