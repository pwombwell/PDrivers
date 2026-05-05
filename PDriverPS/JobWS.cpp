#include "Core/PDriver.h"
#include "JobWS.h"

// Cast parent to child type.
JobWS&
CoreJobWS::jobWS()
{
    return (JobWS&)*this;
}
