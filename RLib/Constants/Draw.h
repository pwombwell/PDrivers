#ifndef RLIB_CONSTANTS_DRAW_H
#define RLIB_CONSTANTS_DRAW_H

/* Constants from Hdr/Draw. */
#define Draw_LgScalingFactor 8u

/* Draw vector reason codes. */
#define DrawReas_ProcessPath 0u
#define DrawReas_ProcessPathFP 1u
#define DrawReas_Fill 2u
#define DrawReas_FillFP 3u
#define DrawReas_Stroke 4u
#define DrawReas_StrokeFP 5u
#define DrawReas_StrokePath 6u
#define DrawReas_StrokePathFP 7u
#define DrawReas_FlattenPath 8u
#define DrawReas_FlattenPathFP 9u
#define DrawReas_TransformPath 10u
#define DrawReas_TransformPathFP 11u
#define DrawReas_FillClipped 12u
#define DrawReas_FillClippedFP 13u
#define DrawReas_StrokeClipped 14u
#define DrawReas_StrokeClippedFP 15u
#define NumberOfDrawReasons 16u

/* Special values of R7 for Draw_ProcessPath. */
#define DrawSpec_InSitu 0u
#define DrawSpec_Fill 1u
#define DrawSpec_FillBySubpaths 2u
#define DrawSpec_Count 3u
#define NumberOfSpecialDraws 4u

/* Fill styles. */
#define FillStyle_OverallMask 0x3Fu
#define FillStyle_RuleMask 0x03u
#define FillStyle_NonZeroRule 0x00u
#define FillStyle_NegativeOnlyRule 0x01u
#define FillStyle_EvenOddRule 0x02u
#define FillStyle_PositiveOnlyRule 0x03u
#define FillStyle_PlotFullExterior 0x04u
#define FillStyle_PlotExteriorBoundary 0x08u
#define FillStyle_PlotInteriorBoundary 0x10u
#define FillStyle_PlotFullInterior 0x20u

/* ProcessPath flags. */
#define ProcessPath_FlagsMask 0xFE000000u
#define ProcessPath_R7IsBoundingBox 0x02000000u
#define ProcessPath_R7IsThirtyTwoBit 0x04000000u
#define ProcessPath_CloseOpenSubpaths 0x08000000u
#define ProcessPath_Flatten 0x10000000u
#define ProcessPath_Thicken 0x20000000u
#define ProcessPath_Reflatten 0x40000000u
#define ProcessPath_Float 0x80000000u

#endif
