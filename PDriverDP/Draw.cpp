#include "Core/PDriver.h"

#include "JobWS.h"
#include "Private.h"

#include "Core/Constants.h"
#include "Core/InterceptManager.h"
#include "Core/Job.h"
#include "Core/Workspace.h"

#include "RLibX/Draw.h"

static Draw::Unit compute_flatness(JobWS& jobWS,
                                   Draw::Unit flatness)
{
    if (flatness != Draw::Unit::zero())
        return flatness;

    Draw::Unit flat = Draw::Unit::fromRaw(512);
    int32_t scale = (int32_t)jobWS.output().currentXScale();
    if ((int32_t)jobWS.output().currentYScale() > scale)
        scale = (int32_t)jobWS.output().currentYScale();

    int32_t scaled = divide_and_scale(flat.raw() * 180, scale);
    flat = Draw::Unit::fromRaw(scaled >> bufferpix_l2size);
    if (flat == Draw::Unit::zero())
        flat = Draw::Unit::fromRaw(1);

    return flat;
}

static Base::Fixed<16> draw_axis_scale(const JobWS& job_ws, bool x_axis)
{
    Base::Fixed<16> scale = x_axis
        ? job_ws.output().currentXScaleNEW()
        : job_ws.output().currentYScaleNEW();

    int64_t raw = (int64_t)scale.raw() * 2;
    raw = raw >= 0 ? (raw + 90) / 180 : -((-raw + 90) / 180);
    return Base::Fixed<16>::fromRaw((int32_t)raw);
}

static Draw::Matrix prepare_draw_matrix(const JobWS& job,
                                        const Draw::Matrix* matrixPtr)
{
    Draw::Matrix matrix = matrixPtr ? *matrixPtr : Draw::Matrix::identity();

    matrix.translate(-Draw::fromOSUnit(job.usersoffset));
    matrix.scaleX(draw_axis_scale(job, true));
    matrix.scaleY(draw_axis_scale(job, false));
    matrix.translate(-job.output().currentOffset());

    return matrix;
}

static MyError draw_fill(Draw::Path path,
                         uint32_t flags,
                         const Draw::Matrix* matrix,
                         Draw::Unit flatness,
                         CoreJobWS& coreJob)
{
    if (coreJob.disabled & Disabled_NullClip)
        return nullptr;

    JobWS& job = toJobWS(coreJob);
    flatness = compute_flatness(job, flatness);
    Draw::Matrix drawMatrix = prepare_draw_matrix(job, matrix);

    CoreWS& ws = CoreWS::instance();
    ScopedPassthrough scopedPT(ws.m_interceptMgr, Passthrough_Draw);
    return Draw::fill(path, flags, &drawMatrix, flatness);
}

static MyError draw_stroke_internal(Draw::Path path,
                                    uint32_t flags,
                                    const Draw::Matrix* matrix,
                                    Draw::Unit flatness,
                                    Draw::Unit thickness,
                                    const Draw::CapJoin* capJoin,
                                    const Draw::DashPattern* dash,
                                    CoreJobWS& coreJob)
{
    JobWS& job = toJobWS(coreJob);

    if (job.disabled & Disabled_NullClip)
        return nullptr;

    flatness = compute_flatness(job, flatness);
    Draw::Matrix drawMatrix = prepare_draw_matrix(job, matrix);

    CoreWS& ws = CoreWS::instance();
    ScopedPassthrough scopedPT(ws.m_interceptMgr, Passthrough_Draw);
    return Draw::stroke(path, flags, &drawMatrix, flatness,
                        thickness, capJoin, dash);
}

MyError draw_boundaryonly(Draw::Path path,
                          const Draw::Matrix* matrix,
                          Draw::Unit flatness,
                          CoreJobWS& coreJob)
{
    return draw_fill(path, 0x18u, matrix, flatness, coreJob);
}

MyError draw_interiornobdry(Draw::Path path,
                            uint32_t flags,
                            const Draw::Matrix* matrix,
                            Draw::Unit flatness,
                            CoreJobWS& coreJob)
{
    uint32_t fill_flags = flags | 0x20u;
    return draw_fill(path, fill_flags, matrix, flatness, coreJob);
}

MyError draw_interior(Draw::Path path,
                      uint32_t flags,
                      const Draw::Matrix* matrix,
                      Draw::Unit flatness,
                      CoreJobWS& coreJob)
{
    uint32_t fill_flags = flags | 0x30u;
    return draw_fill(path, fill_flags, matrix, flatness, coreJob);
}

MyError draw_interiorwithbdry(Draw::Path path,
                              uint32_t flags,
                              const Draw::Matrix* matrix,
                              Draw::Unit flatness,
                              CoreJobWS& coreJob)
{
    uint32_t fill_flags = flags | 0x38u;
    return draw_fill(path, fill_flags, matrix, flatness, coreJob);
}

MyError draw_strokenobdry(Draw::Path path,
                          const Draw::Matrix* matrix,
                          Draw::Unit flatness,
                          Draw::Unit thickness,
                          const Draw::CapJoin* capJoin,
                          const Draw::DashPattern* dash,
                          CoreJobWS& coreJob)
{
    return draw_stroke_internal(path, 0x20u, matrix, flatness,
                                thickness, capJoin, dash, coreJob);
}

MyError draw_stroke(Draw::Path path,
                    const Draw::Matrix* matrix,
                    Draw::Unit flatness,
                    Draw::Unit thickness,
                    const Draw::CapJoin* capJoin,
                    const Draw::DashPattern* dash,
                    CoreJobWS& coreJob)
{
    return draw_stroke_internal(path, 0x30u, matrix, flatness,
                                thickness, capJoin, dash, coreJob);
}

MyError draw_strokewithbdry(Draw::Path path,
                            const Draw::Matrix* matrix,
                            Draw::Unit flatness,
                            Draw::Unit thickness,
                            const Draw::CapJoin* capJoin,
                            const Draw::DashPattern* dash,
                            CoreJobWS& coreJob)
{
    return draw_stroke_internal(path, 0x38u, matrix, flatness,
                                thickness, capJoin, dash, coreJob);
}

MyError draw_thinstroke(Draw::Path path,
                        const Draw::Matrix* matrix,
                        Draw::Unit flatness,
                        const Draw::DashPattern* dash,
                        CoreJobWS& coreJob)
{
    return draw_stroke_internal(path, 0u, matrix, flatness, Draw::Unit::zero(),
                                nullptr, dash, coreJob);
}
