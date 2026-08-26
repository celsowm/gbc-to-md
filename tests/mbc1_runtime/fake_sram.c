#include "sram.h"
#include <assert.h>
static u8 fake_sram[32768];
static int enabled;
void SRAM_enable(void){ enabled=1; }
void SRAM_enableRO(void){ enabled=2; }
void SRAM_disable(void){ enabled=0; }
u8 SRAM_readByte(u32 off){ assert(enabled && off<sizeof(fake_sram)); return fake_sram[off]; }
void SRAM_writeByte(u32 off,u8 v){ assert(enabled==1 && off<sizeof(fake_sram)); fake_sram[off]=v; }
