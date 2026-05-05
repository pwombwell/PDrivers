#include "Core/PDriver.h"
#include "Picture.h"

#include "Core/MsgCode.h"
#include "Core/Workspace.h"

#include "RLib/OS/Error.h"

MyError picture_insert(FileHandle insertFrom,
                       const Draw::Path* clipPath,
                       const Point<OS::Unit>& bottomLeft,
                       const Point<OS::Unit>& bottomRight,
                       const Point<OS::Unit>& topLeft)
{
    CoreWS& ws = CoreWS::instance();
    return ws.messages.lookupError(ErrorBlock_PrintNoIncludedFiles, "PDriverPS");
}
