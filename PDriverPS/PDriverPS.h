#ifndef PDRIVERPS_PDRIVERPS_H
#define PDRIVERPS_PDRIVERPS_H

/* PDriverPS settings mirrored from PDriverPS.s */

#ifndef PrinterType
#define PrinterType "PostScript"
#endif
#ifndef PrinterNumber
#define PrinterNumber 0
#endif
#ifndef ModSuffix
#define ModSuffix "PS"
#endif
#ifndef DirSuffix
#define DirSuffix "PS"
#endif

#define BeingDeveloped 0
#define DevelopmentChecks 0

#define UsePDriverPath 1

#define PSCoordSpeedUps 1
#define PSTextSpeedUps 1
#define PSFreeFlatness 1

#define PSSprColLimit 12

#define PSAllowHighTables 1
#define PSSprUseBBoxes 1
#define PSSprFillChunk 0
#define PSSprInverted 0

#define PSSprRLEncode 1
#define PSSprRLMaxStr 128
#define PSSprRLMaxStrL2 128
#define PSSprRLMinRun 5
#define PSSprRLMinRunL2 3

#define PSFlushAtEOL 1
#define PSAxisAlignedJustification 1

#define PSDebug 0
#define PSDebugManageJ 0
#define PSDebugPageBox 0
#define PSDebugColour 0
#define PSDebugPlot 0
#define PSDebugFont 0
#define PSDebugEscapes 0
#define PSDebugBPuts 0
#define PSDebugSprite 0

#define debugPageBox 0
#define debugSprite 1
#define debugTransSprite 1
#define debugFont 0
#define debugColour 0

/* This module uses the shared printer driver Messages file only. */
// #define PrivMessages "..."

#endif
