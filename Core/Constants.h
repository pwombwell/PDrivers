#ifndef CORE_CONSTANTS_H
#define CORE_CONSTANTS_H

#include "RLibX/Utils/EnumBitmaskOps.h"

/* Core.Constants -> C */

#define TopBit 0x80000000u

#define BigNum 0x02000000u

/* Flags used by Font_Paint. */
enum FontPaintFlag /* : uint32_t */ {
    FontPaintFlag_None          =      0,
    FontPaintFlag_Justify       =  1u<<0,
    FontPaintFlag_Rubout        =  1u<<1,
    FontPaintFlag_Mpoint        =  1u<<4,
    FontPaintFlag_CoordsBlk     =  1u<<5,
    FontPaintFlag_Matrix        =  1u<<6,
    FontPaintFlag_Length        =  1u<<7,
    FontPaintFlag_UseHandle     =  1u<<8,
    FontPaintFlag_Kern          =  1u<<9,
    FontPaintFlag_Reversed      = 1u<<10,
    FontPaintFlag_Blended       = 1u<<11,
    FontPaintFlag_16bit         = 1u<<12,
    FontPaintFlag_32bit         = 1u<<13,
    FontPaintFlag_Blendsupr     = 1u<<14
};
DEFINE_ENUM_BITWISE_OPERATORS(FontPaintFlag);

/* Flags used by Font_ScanString. */
#define fontscanflag_findcaret (1u << 17)
#define fontscanflag_rtnbbox (1u << 18)
#define fontscanflag_rtnmatrix (1u << 19)
#define fontscanflag_rtnsplit (1u << 20)

/* Flags used by ColourTrans_SelectTable. */
#define selecttableflag_AbsoluteSprite (1u << 0)
#define selecttableflag_UseCurrent (1u << 1)

/* Buffer sizes. */
#define errorbufferlen 256u
#define globalerrorbufferlen 256u
#define textbufferlen 256u

#define XPDriver_DeclareDriver      0xA0156u
#define XPDriver_RemoveDriver       0xA0157u

#if 0
/* VDU variable numbers used by OS_ReadModeVariable/OS_ReadVduVariables. */
#define VduExt_ModeFlags                 0u
#define VduExt_ScrRCol                   1u
#define VduExt_ScrBRow                   2u
#define VduExt_NColour                   3u
#define VduExt_XEigFactor                4u
#define VduExt_YEigFactor                5u
#define VduExt_LineLength                6u
#define VduExt_ScreenSize                7u
#define VduExt_YShiftSize                8u
#define VduExt_Log2BPP                   9u
#define VduExt_Log2BPC                  10u
#define VduExt_XWindLimit               11u
#define VduExt_YWindLimit               12u
#define VduExt_MinScreenBanks           13u
#define VduExt_GWLCol                  128u
#define VduExt_GWBRow                  129u
#define VduExt_GWRCol                  130u
#define VduExt_GWTRow                  131u
#define VduExt_TWLCol                  132u
#define VduExt_TWBRow                  133u
#define VduExt_TWRCol                  134u
#define VduExt_TWTRow                  135u
#define VduExt_OrgX                    136u
#define VduExt_OrgY                    137u
#define VduExt_GCsX                    138u
#define VduExt_GCsY                    139u
#define VduExt_OlderCsX                140u
#define VduExt_OlderCsY                141u
#define VduExt_OldCsX                  142u
#define VduExt_OldCsY                  143u
#define VduExt_GCsIX                   144u
#define VduExt_GCsIY                   145u
#define VduExt_NewPtX                  146u
#define VduExt_NewPtY                  147u
#define VduExt_ScreenStart             148u
#define VduExt_DisplayStart            149u
#define VduExt_TotalScreenSize         150u
#define VduExt_GPLFMD                  151u
#define VduExt_GPLBMD                  152u
#define VduExt_GFCOL                   153u
#define VduExt_GBCOL                   154u
#define VduExt_TForeCol                155u
#define VduExt_TBackCol                156u
#define VduExt_GFTint                  157u
#define VduExt_GBTint                  158u
#define VduExt_TFTint                  159u
#define VduExt_TBTint                  160u
#define VduExt_MaxMode                 161u
#define VduExt_GCharSizeX              162u
#define VduExt_GCharSizeY              163u
#define VduExt_GCharSpaceX             164u
#define VduExt_GCharSpaceY             165u
#define VduExt_HLineAddr               166u
#define VduExt_TCharSizeX              167u
#define VduExt_TCharSizeY              168u
#define VduExt_TCharSpaceX             169u
#define VduExt_TCharSpaceY             170u
#define VduExt_GcolOraEorAddr          171u
#define VduExt_VIDCClockSpeed          172u
#define VduExt_PixelRate               173u
#define VduExt_BorderL                 174u
#define VduExt_BorderB                 175u
#define VduExt_BorderR                 176u
#define VduExt_BorderT                 177u
#define VduExt_LeftBorderSize          VduExt_BorderL
#define VduExt_BottomBorderSize        VduExt_BorderB
#define VduExt_RightBorderSize         VduExt_BorderR
#define VduExt_TopBorderSize           VduExt_BorderT
#define VduExt_CurrentGraphicsVDriver  192u
#define VduExt_WindowWidth             256u
#define VduExt_WindowHeight            257u
#endif

/* Bits in ModeFlags (Hdr:VduExt). */
#define ModeFlag_NonGraphic                     (1u << 0)
#define ModeFlag_Teletext                       (1u << 1)
#define ModeFlag_GapMode                        (1u << 2)
#define ModeFlag_BBCGapMode                     (1u << 3)
#define ModeFlag_HiResMono                      (1u << 4)
#define ModeFlag_DoubleVertical                 (1u << 5)
#define ModeFlag_HardScrollDisabled             (1u << 6)
#define ModeFlag_FullPalette                    (1u << 7)
#define ModeFlag_64k                            ModeFlag_FullPalette
#define ModeFlag_InterlacedMode                 (1u << 8)
#define ModeFlag_GreyscalePalette               (1u << 9)
#define ModeFlag_ChromaSubsampleMode            ModeFlag_GreyscalePalette
#define ModeFlag_DataFormat_Mask              (0xFu << 12)
#define ModeFlag_DataFormatFamily_Mask          (3u << 12)
#define ModeFlag_DataFormatFamily_RGB           (0u << 12)
#define ModeFlag_DataFormatFamily_Misc          (1u << 12)
#define ModeFlag_DataFormatFamily_YCbCr         (2u << 12)
#define ModeFlag_DataFormatSub_Mask           (0xCu << 12)
#define ModeFlag_DataFormatSub_RGB              (4u << 12)
#define ModeFlag_DataFormatSub_Alpha            (8u << 12)
#define ModeFlag_DataFormatSub_Video            (4u << 12)
#define ModeFlag_DataFormatSub_709              (8u << 12)
#define ModeFlag_Transform_Mask                 (7u << 16)
#define ModeFlag_Transform_Rotate90             (1u << 16)
#define ModeFlag_Transform_Rotate180            (2u << 16)
#define ModeFlag_Transform_VFlip                (4u << 16)

// From Sources/Programmer/HdrSrc/hdr/NewErrors
#define ErrorNumber_Escape                      17

#define ErrorNumber_HeapBase                   0x180
#define ErrorNumber_HeapFail_Alloc              (ErrorNumber_HeapBase + 0x04)

#define ErrorNumber_PrintBase                   0x5C0
#define ErrorNumber_PrintBadFeatures            (ErrorNumber_PrintBase + 0x00)
#define ErrorNumber_PrintNoCurrentSprite        (ErrorNumber_PrintBase + 0x01)
#define ErrorNumber_PrintNoJobSelected          (ErrorNumber_PrintBase + 0x02)
#define ErrorNumber_PrintNoSuchJob              (ErrorNumber_PrintBase + 0x03)
#define ErrorNumber_PrintNoCurrentPage          (ErrorNumber_PrintBase + 0x04)
#define ErrorNumber_PrintPrintingPage           (ErrorNumber_PrintBase + 0x05)
#define ErrorNumber_PrintInvalidCopies          (ErrorNumber_PrintBase + 0x06)
#define ErrorNumber_PrintCannotHandle           (ErrorNumber_PrintBase + 0x07)
#define ErrorNumber_PrintBadHalftone            (ErrorNumber_PrintBase + 0x08)
#define ErrorNumber_PrintCancelled              (ErrorNumber_PrintBase + 0x09)
#define ErrorNumber_PrintSingularMatrix         (ErrorNumber_PrintBase + 0x0a)
#define ErrorNumber_PrintBadRectangle           (ErrorNumber_PrintBase + 0x0b)
#define ErrorNumber_PrintRectanglesMiss         (ErrorNumber_PrintBase + 0x0c)
#define ErrorNumber_PrintNoFreeMemory           (ErrorNumber_PrintBase + 0x0d)
#define ErrorNumber_PrintNotOnePage             (ErrorNumber_PrintBase + 0x0e)
#define ErrorNumber_PrintInUse                  (ErrorNumber_PrintBase + 0x0f)
#define ErrorNumber_PrintOverflow               (ErrorNumber_PrintBase + 0x10)
#define ErrorNumber_PrintBadMiscOp              (ErrorNumber_PrintBase + 0x11)
#define ErrorNumber_PrintNoDuplicates           (ErrorNumber_PrintBase + 0x12)

#define ErrorNumber_PrintNoColour               ErrorNumber_PrintBadFeatures
#define ErrorNumber_PrintColourNotConfig        ErrorNumber_PrintBadFeatures
#define ErrorNumber_PrintNotFullColour          ErrorNumber_PrintBadFeatures
#define ErrorNumber_PrintDiscreteColours        ErrorNumber_PrintBadFeatures
#define ErrorNumber_PrintBadFills               ErrorNumber_PrintBadFeatures
#define ErrorNumber_PrintBadThickLines          ErrorNumber_PrintBadFeatures
#define ErrorNumber_PrintNoOverwrite            ErrorNumber_PrintBadFeatures
#define ErrorNumber_PrintNoScreenDump           ErrorNumber_PrintBadFeatures
#define ErrorNumber_PrintBadTransform           ErrorNumber_PrintBadFeatures
#define ErrorNumber_PrintNoIncludedFiles        ErrorNumber_PrintBadFeatures

#define ErrorNumber_PrintNoCurrentDriver        (ErrorNumber_PrintBase + 0x13)
#define ErrorNumber_PrintUnknownNumber          (ErrorNumber_PrintBase + 0x14)
#define ErrorNumber_PrintDuplicateNumber        (ErrorNumber_PrintBase + 0x15)
#define ErrorNumber_PrintBadSetPrinter          (ErrorNumber_PrintBase + 0x16)

#define ErrorNumber_PDumperUndeclared           (ErrorNumber_PrintBase + 0x17)
#define ErrorNumber_PDumperTooOld               (ErrorNumber_PrintBase + 0x18)
#define ErrorNumber_PDumperDuplicateModule      (ErrorNumber_PrintBase + 0x19)
#define ErrorNumber_PDumperBadCall              (ErrorNumber_PrintBase + 0x1a)
#define ErrorNumber_PDumperBadStrip             (ErrorNumber_PrintBase + 0x1b)
#define ErrorNumber_PDumperBadPalette           (ErrorNumber_PrintBase + 0x1c)
#define ErrorNumber_PDumperNotLinked            (ErrorNumber_PrintBase + 0x1d)
#define ErrorNumber_PDumperReserved             (ErrorNumber_PrintBase + 0x1e)
#define ErrorNumber_PDumperBadOutputType        (ErrorNumber_PrintBase + 0x1f)
#define ErrorNumber_PDumperBlockNotFound        (ErrorNumber_PrintBase + 0x20)
#define ErrorNumber_PDumperInUse                (ErrorNumber_PrintBase + 0x21)

#define ErrorNumber_PrintCantPrinterVDU         ErrorNumber_PrintCannotHandle
#define ErrorNumber_PrintCantVDU4               ErrorNumber_PrintCannotHandle
#define ErrorNumber_PrintCantModeChange         ErrorNumber_PrintCannotHandle
#define ErrorNumber_PrintCantThisVDU23          ErrorNumber_PrintCannotHandle
#define ErrorNumber_PrintCantHorizFill          ErrorNumber_PrintCannotHandle
#define ErrorNumber_PrintCantFloodFill          ErrorNumber_PrintCannotHandle
#define ErrorNumber_PrintCantCopyMove           ErrorNumber_PrintCannotHandle
#define ErrorNumber_PrintCantUndefPlot          ErrorNumber_PrintCannotHandle
#define ErrorNumber_PrintCantFontSpriteVDU      ErrorNumber_PrintCannotHandle
#define ErrorNumber_PrintCantUnkColV            ErrorNumber_PrintCannotHandle
#define ErrorNumber_PrintCantDrawPlot           ErrorNumber_PrintCannotHandle
#define ErrorNumber_PrintCantThisFill           ErrorNumber_PrintCannotHandle
#define ErrorNumber_PrintCantUnkSpriteOp        ErrorNumber_PrintCannotHandle
#define ErrorNumber_PrintCantThisSpriteOp       ErrorNumber_PrintCannotHandle
#define ErrorNumber_PrintCantThisFontPaint      ErrorNumber_PrintCannotHandle

#define ErrorNumber_PrintJPEGNoSupp             (ErrorNumber_PrintBase + 0x22)
#define ErrorNumber_PrintJPEGOldSprEx           (ErrorNumber_PrintBase + 0x23)
#define ErrorNumber_PDumperEscDisabled          (ErrorNumber_PrintBase + 0x24)

#define ErrorNumber_DrawBase                   0x980
#define ErrorNumber_NoDrawInIRQMode            (ErrorNumber_DrawBase + 0x00)
#define ErrorNumber_BadDrawReasonCode          (ErrorNumber_DrawBase + 0x01)
#define ErrorNumber_ReservedDrawBits           (ErrorNumber_DrawBase + 0x02)
#define ErrorNumber_InvalidDrawAddress         (ErrorNumber_DrawBase + 0x03)
#define ErrorNumber_BadPathElement             (ErrorNumber_DrawBase + 0x04)
#define ErrorNumber_BadPathSequence            (ErrorNumber_DrawBase + 0x05)
#define ErrorNumber_MayExpandPath              (ErrorNumber_DrawBase + 0x06)
#define ErrorNumber_PathFull                   (ErrorNumber_DrawBase + 0x07)
#define ErrorNumber_PathNotFlat                (ErrorNumber_DrawBase + 0x08)
#define ErrorNumber_BadCapsOrJoins             (ErrorNumber_DrawBase + 0x09)
#define ErrorNumber_TransformOverflow          (ErrorNumber_DrawBase + 0x0a)
#define ErrorNumber_DrawNeedsGraphicsMode      (ErrorNumber_DrawBase + 0x0b)

#define ErrorNumber_MessageTransBase           0xac0
#define ErrorNumber_MessageTrans_Syntax        (ErrorNumber_MessageTransBase + 0x00)         
#define ErrorNumber_MessageTrans_FileOpen      (ErrorNumber_MessageTransBase + 0x01)             
#define ErrorNumber_MessageTrans_TokenNotFound (ErrorNumber_MessageTransBase + 0x02)                 
#define ErrorNumber_MessageTrans_Recurse       (ErrorNumber_MessageTransBase + 0x03)             
#define ErrorNumber_MessageTrans_BadDesc       (ErrorNumber_MessageTransBase + 0x04)             

#define PDriverType_PS                   0
#define PDriverType_DM                   1
#define PDriverType_LJ                   2
#define PDriverType_IX                   3
#define PDriverType_FX                   4
#define PDriverType_LZ                   5
#define PDriverType_LB                   6
#define PDriverType_DP                   7
#define PDriverType_JP                   8
#define PDriverType_DJ                   9
#define PDriverType_CCBJ10              10
#define PDriverType_CCBJC800            11
#define PDriverType_CCDJ                12
#define PDriverType_CDJ500C             13
#define PDriverType_CCIX                14
#define PDriverType_CCBJ200             15
#define PDriverType_CCCanonBubbleJet    16
#define PDriverType_CCCanonNative       17
#define PDriverType_CCHPPCL             18
#define PDriverType_CCEpsonEscP2        19
#define PDriverType_AF                  20
#define PDriverType_JX                  99
#define PDriverType_PJ                  99

#define PDumperSP_Number                0
#define PDumperDM_Number                1
#define PDumperLJ_Number                2
#define PDumperIW_Number                3
#define PDumper24_Number                4
#define PDumperDJ_Number                5
#define PDumperE2_Number                6
#define PDumperLB_Number                7
#define PDumperAF_Number                8
#define PDumperOS_Number                9
#define PDumperFP_Number                10
#define PDumperCX_Number                11
#define PDumperCP_Number                12
#define PDumperLM_Number                13
#define PDumperCX2_Number               14
#define PDumperLZ11_Number              15
#define PDumperLZ12_Number              16
#define PDumperHPS_Number               17
#define PDumperHP_Multi                 18
#define PDumperUP_Number                19
#define PDumperPC_Number                20
#define PDumperGP_Number                21
#define PDumperEpsonESCi_Number         22
#define PDumperEK_Number                23
#define PDumperPCL_Number               24
#define PDumperSP25_Number              25
#define PDumperIPP_Number               26
#define PDumperURF_Number               27

#define PDriverMiscOp_PS_Font       1    // These are types of PostScript resource
#define PDriverMiscOp_PS_File       2    // as seen in DSC 3.00.
#define PDriverMiscOp_PS_ProcSet    3
#define PDriverMiscOp_PS_Pattern    4
#define PDriverMiscOp_PS_Form       5
#define PDriverMiscOp_PS_Encoding   6

// The following are NOT official PostScript resource types
#define PDriverMiscOp_PS_DF		   40 // DF is for the DocumentFonts list
#define PDriverMiscOp_PS_DSF	   41 // DSF is for the DocumentSuppliedFonts

#endif
