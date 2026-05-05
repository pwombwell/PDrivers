#include "ErrorBlocks.h"

#include "RLib/OS/Error.h"

MAKE_INTERNAT_ERROR_BLOCK(Escape, "Escape");
MAKE_INTERNAT_ERROR_BLOCK(HeapFail_Alloc, "HeapFailAlloc");

MAKE_INTERNAT_ERROR_BLOCK(PrintBadFeatures, "BadFeat");
MAKE_INTERNAT_ERROR_BLOCK(PrintNoJobSelected, "NoJbSel");
MAKE_INTERNAT_ERROR_BLOCK(PrintNoSuchJob, "NoJob");
MAKE_INTERNAT_ERROR_BLOCK(PrintNoCurrentPage, "NoPage");
MAKE_INTERNAT_ERROR_BLOCK(PrintPrintingPage, "PrintP");
MAKE_INTERNAT_ERROR_BLOCK(PrintInvalidCopies, "BadCops");
MAKE_INTERNAT_ERROR_BLOCK(PrintBadHalftone, "BadHton");
MAKE_INTERNAT_ERROR_BLOCK(PrintCancelled, "PCancel");
MAKE_INTERNAT_ERROR_BLOCK(PrintNotOnePage, "NotOneP");
MAKE_INTERNAT_ERROR_BLOCK(PrintOverflow, "BufOFlo");
MAKE_INTERNAT_ERROR_BLOCK(PrintBadMiscOp, "BadMOp");
MAKE_INTERNAT_ERROR_BLOCK(PrintNoDuplicates, "NoDup");
MAKE_INTERNAT_ERROR_BLOCK(PrintNoColour, "NoCol");
MAKE_INTERNAT_ERROR_BLOCK(PrintColourNotConfig,"NotConf");
MAKE_INTERNAT_ERROR_BLOCK(PrintNoScreenDump, "NoSDump");
MAKE_INTERNAT_ERROR_BLOCK(PrintBadTransform, "BadTran");
MAKE_INTERNAT_ERROR_BLOCK(PrintRectanglesMiss, "RecMiss");
MAKE_INTERNAT_ERROR_BLOCK(PrintNoFreeMemory, "NoMem");
MAKE_INTERNAT_ERROR_BLOCK(PrintNoIncludedFiles, "NoIncl");
MAKE_INTERNAT_ERROR_BLOCK(PrintBadSetPrinter, "NoPDSet");
MAKE_INTERNAT_ERROR_BLOCK(PrintJPEGNoSupp, "JPNoSup");
MAKE_INTERNAT_ERROR_BLOCK(PrintInUse, "PDrUsed");
MAKE_INTERNAT_ERROR_BLOCK(PDumperUndeclared, "PDUndec");
MAKE_INTERNAT_ERROR_BLOCK(PDumperTooOld, "TooOld");
MAKE_INTERNAT_ERROR_BLOCK(PDumperDuplicateModule, "DupPDNo");
MAKE_INTERNAT_ERROR_BLOCK(PDumperBadStrip, "BadStrp");
MAKE_INTERNAT_ERROR_BLOCK(PDumperInUse, "PDuUsed");
MAKE_INTERNAT_ERROR_BLOCK(PrintCantPrinterVDU, "NoVDU");
MAKE_INTERNAT_ERROR_BLOCK(PrintCantVDU4, "NoVDU4");
MAKE_INTERNAT_ERROR_BLOCK(PrintCantModeChange, "NoModCh");
MAKE_INTERNAT_ERROR_BLOCK(PrintCantThisVDU23, "NoVDU23");
MAKE_INTERNAT_ERROR_BLOCK(PrintCantHorizFill, "NoHFill");
MAKE_INTERNAT_ERROR_BLOCK(PrintCantFloodFill, "NoFFill");
MAKE_INTERNAT_ERROR_BLOCK(PrintCantCopyMove, "NoBlkOp");
MAKE_INTERNAT_ERROR_BLOCK(PrintCantUndefPlot, "NoUPlot");
MAKE_INTERNAT_ERROR_BLOCK(PrintCantDrawPlot, "NoDPlot");
MAKE_INTERNAT_ERROR_BLOCK(PrintCantThisFill, "NoFill");
MAKE_INTERNAT_ERROR_BLOCK(PrintCantThisFontPaint, "NoFPnt");
MAKE_INTERNAT_ERROR_BLOCK(PrintCantThisSpriteOp, "NoSprOp");

MAKE_INTERNAT_ERROR_BLOCK(BadPathElement, "PathEl");
MAKE_INTERNAT_ERROR_BLOCK(BadPathSequence, "PathSeq");

#if !DoFontSpriteVdu
MAKE_INTERNAT_ERROR_BLOCK(PrintCantFontSpriteVDU, "NoFSVDU");
#endif

#if VectorErrors
MAKE_INTERNAT_ERROR_BLOCK(PrintCantUnkColV, "PrintCantUnkColV");
#endif
