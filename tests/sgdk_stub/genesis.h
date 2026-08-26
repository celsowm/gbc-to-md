#ifndef GENESIS_H
#define GENESIS_H
#ifdef SGDK_GCC
typedef char s8;
typedef short s16;
typedef long s32;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef u8 bool;
#define false 0
#define true 1
#define uint8_t u8
#define int8_t s8
#define uint16_t u16
#define int16_t s16
#define uint32_t u32
#define int32_t s32
#define size_t u32
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int16_t s16;
#endif
typedef int VDPPlane; typedef int TransferMethod;
#define TRUE true
#define FALSE false
#define CPU 0
#define DMA_QUEUE 1
#define DMA_QUEUE_COPY 2
#define BG_A 0
#define JOY_1 0
#define TILE_USER_INDEX 16
#define BUTTON_UP (1u<<0)
#define BUTTON_DOWN (1u<<1)
#define BUTTON_LEFT (1u<<2)
#define BUTTON_RIGHT (1u<<3)
#define BUTTON_A (1u<<4)
#define BUTTON_B (1u<<5)
#define BUTTON_C (1u<<6)
#define BUTTON_START (1u<<7)
#define SPRITE_SIZE(w,h) ((u8)(0u * (u8)(w) + 0u * (u8)(h)))
#define TILE_ATTR_FULL(pal,prio,vf,hf,tile) ((u16)((tile) + 0u*(pal) + 0u*(prio) + 0u*(vf) + 0u*(hf)))
#define RGB24_TO_VDPCOLOR(c) ((u16)(0u * (u32)(c)))
void VDP_loadTileData(const u32 *data, u16 index, u16 num, TransferMethod tm);
void VDP_setTileMapXY(VDPPlane plane, u16 tile, u16 x, u16 y);
void VDP_setTileMapDataRowEx(VDPPlane plane, const u16 *data, u16 basetile, u16 row, u16 x, u16 w, TransferMethod tm);
void VDP_setSpriteFull(u16 index, s16 x, s16 y, u8 size, u16 attribut, u8 link);
void VDP_setHorizontalScroll(VDPPlane plane, s16 value);
void VDP_setVerticalScroll(VDPPlane plane, s16 value);
u16 JOY_readJoypad(u16 joy);
void PAL_setColors(u16 index, const u16 *pal, u16 count, TransferMethod tm);
void VDP_clearPlane(VDPPlane plane, bool wait);
void VDP_clearSprites(void);
void VDP_updateSprites(u16 num, TransferMethod tm);
void SYS_doVBlankProcess(void);
#endif
