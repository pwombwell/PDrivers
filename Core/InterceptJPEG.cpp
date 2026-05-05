#include "PDriver.h"

#include "Device.h"
#include "Job.h"
#include "InterceptJPEG.h"
#include "MsgCode.h"
#include "OS.h"
#include "Workspace.h"

#include "RLib/Draw.h"
#include "RLib/Geometry.h"

#undef JPEG_Info
#define JPEG_Info 0x00049980
#undef JPEG_PlotScaled
#define JPEG_PlotScaled 0x00049982
#undef JPEG_PlotFileScaled
#define JPEG_PlotFileScaled 0x00049983
#undef JPEG_PlotTransformed
#define JPEG_PlotTransformed 0x00049984
#undef JPEG_PlotFileTransformed
#define JPEG_PlotFileTransformed 0x00049985

MyError
InterceptJPEG::plotScaled(const uint8_t* jpeg, uint32_t len,
                          uint32_t flags,
                          OS::Unit x, OS::Unit y,
                          const Sprite::ScaleFactor* scale)
{
    MyError err;

    if ((err = m_job.checkPersistentError()) != nullptr)
        return persistentReturn(err);

    if ((err = jpeg_plotscaled(jpeg, len, flags, x, y, scale, m_job)) != nullptr)
        return errorReturn(err);

    return nullptr;
}

MyError
InterceptJPEG::plotTransformed(const uint8_t* jpeg, uint32_t len,
                               uint32_t flags, const int32_t* matrixOrCoords)
{
    MyError err;

    if ((err = m_job.checkPersistentError()) != nullptr)
        return persistentReturn(err);

    // have to adjust positional parts of matrix or dest. coords
    // according to origin and useroffset
    if (!!(flags & 1u)) {
        // `jpegswi_pt_coords`
        const Sprite::Coords* srcCoords((const Sprite::Coords*)matrixOrCoords);
        Sprite::Coords coords;

        for (int i = 0; i < 4; i++) {
            Point<Draw::Unit> point((*srcCoords)[i]);
            point += Draw::fromOSUnit(m_job.origin); // norcroft moaned if one line
            point -= Draw::fromOSUnit(m_job.usersoffset);
            coords.points[i] = point;
        }

        err = jpeg_plottransformed(jpeg, len, coords, m_job);
    } else {
        Draw::Matrix matrix(Draw::Matrix::fromRaw6(matrixOrCoords));
        matrix.e += m_job.origin.dx - m_job.usersoffset.dx;
        matrix.f += m_job.origin.dy - m_job.usersoffset.dy;

        err = jpeg_plottransformed(jpeg, len, matrix, m_job);
    }

    if (err)
        return errorReturn(err);

    return nullptr;
}

MyError
InterceptJPEG::unsupported()
{
    MyError err = m_ws.messages.lookupError(ErrorBlock_PrintJPEGNoSupp);

    return m_job.makePersistentError(err);
}

MyError InterceptJPEG::errorReturn(MyError err)
{
    m_ws.jpeg_ctransflag = false;
    return err;
}

static const char* test_char(const OS::Regs& regs)
{
    return regs.as<const char*>(0);
}

static const unsigned char* test_uchar(const OS::Regs& regs)
{
    return regs.as<const unsigned char*>(0);
}

static const uint8_t* test_uint8(const OS::Regs& regs)
{
    return regs.as<const uint8_t*>(0);
}

static const Sprite::ScaleFactor* test_scale(const OS::Regs& regs)
{
    return regs.as<const Sprite::ScaleFactor*>(0);
}

MyError InterceptJPEG::intercept(const OS::Regs& regs)
{
    const uint8_t* jpeg(regs.as<const uint8_t*>(0));
    uint32_t reason = regs[8];

    switch (reason) {
    case JPEG_PlotScaled: {
        const Sprite::ScaleFactor* scale = regs.as<const Sprite::ScaleFactor*>(3);
        int32_t x = regs[1];
        int32_t y = regs[2];
        uint32_t flags = regs[5];
        uint32_t len = regs[4];
        return plotScaled(jpeg, len, flags, x, y, scale);
    }

    case JPEG_PlotTransformed: {
        const int32_t* matrixOrCoords = regs.as<const int32_t*>(2);
        uint32_t flags = regs[1];
        uint32_t len = regs[3];
        return plotTransformed(jpeg, len, flags, matrixOrCoords);
    }

    case JPEG_PlotFileScaled:
    case JPEG_PlotFileTransformed:
        return unsupported().toKernelOSError();

    default:
        // Original ARM doesn't fault unknown plot SWIs and just returns.
        break;
    }

    return nullptr;
}
