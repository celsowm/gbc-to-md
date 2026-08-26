#ifndef SRAM_H
#define SRAM_H
#include <stdint.h>
typedef uint8_t u8; typedef uint32_t u32;
void SRAM_enable(void);
void SRAM_enableRO(void);
void SRAM_disable(void);
u8 SRAM_readByte(u32 offset);
void SRAM_writeByte(u32 offset,u8 value);
#endif
