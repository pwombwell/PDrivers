#ifndef RLIB_CONSTANTS_SPRITE_H
#define RLIB_CONSTANTS_SPRITE_H

// SpriteOp reason codes (Hdr:Sprite).
#define SpriteReason_ScreenSave 2u
#define SpriteReason_ScreenLoad 3u

#define SpriteReason_ReadAreaCB 8u
#define SpriteReason_ClearSprites 9u
#define SpriteReason_LoadSpriteFile 10u
#define SpriteReason_MergeSpriteFile 11u
#define SpriteReason_SaveSpriteFile 12u
#define SpriteReason_ReturnName 13u
#define SpriteReason_GetSprite 14u
#define SpriteReason_CreateSprite 15u
#define SpriteReason_GetSpriteUserCoords 16u
#define SpriteReason_CheckSpriteArea 17u

#define SpriteReason_SelectSprite 24u
#define SpriteReason_DeleteSprite 25u
#define SpriteReason_RenameSprite 26u
#define SpriteReason_CopySprite 27u
#define SpriteReason_PutSprite 28u
#define SpriteReason_CreateMask 29u
#define SpriteReason_RemoveMask 30u
#define SpriteReason_InsertRow 31u
#define SpriteReason_DeleteRow 32u
#define SpriteReason_FlipAboutXAxis 33u
#define SpriteReason_PutSpriteUserCoords 34u
#define SpriteReason_AppendSprite 35u
#define SpriteReason_SetPointerShape 36u
#define SpriteReason_CreateRemovePalette 37u
#define SpriteReason_CreateRemoveAlpha 38u

#define SpriteReason_ReadSpriteSize 40u
#define SpriteReason_ReadPixelColour 41u
#define SpriteReason_WritePixelColour 42u
#define SpriteReason_ReadPixelMask 43u
#define SpriteReason_WritePixelMask 44u
#define SpriteReason_InsertCol 45u
#define SpriteReason_DeleteCol 46u
#define SpriteReason_FlipAboutYAxis 47u
#define SpriteReason_PlotMask 48u
#define SpriteReason_PlotMaskUserCoords 49u

#define SpriteReason_PlotMaskScaled 50u
#define SpriteReason_PaintCharScaled 51u
#define SpriteReason_PutSpriteScaled 52u
#define SpriteReason_PutSpriteGreyScaled 53u
#define SpriteReason_RemoveLeftHandWastage 54u
#define SpriteReason_PlotMaskTransformed 55u
#define SpriteReason_PutSpriteTransformed 56u
#define SpriteReason_InsertDeleteRows 57u
#define SpriteReason_InsertDeleteColumns 58u

#define SpriteReason_SwitchOutputToSprite 60u
#define SpriteReason_SwitchOutputToMask 61u
#define SpriteReason_ReadSaveAreaSize 62u
#define SpriteReason_TileSpriteScaled 65u

#define SpriteReason_BadReasonCode 66u

#endif
