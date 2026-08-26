#include <mapper.h>
#include <stdint.h>

static u16 banks[8] = {0,1,2,3,4,5,6,7};

u16 SYS_getBank(u16 regionIndex) {
    return regionIndex < 8u ? banks[regionIndex] : 0u;
}

void SYS_setBank(u16 regionIndex, u16 bankIndex) {
    if (regionIndex > 0u && regionIndex < 8u) banks[regionIndex] = bankIndex;
}

void* SYS_getFarDataEx(void* data, bool high) {
    const uintptr_t logical=(uintptr_t)data;
    const u16 region=high ? 7u : 6u;
    const u16 bank=(u16)((logical >> 19) & 0x3Fu);
    SYS_setBank(region,bank);
    /* Host proof keeps the entire low-address ROM mmap visible. The target
       would return 0x380000 + in-bank offset; the runtime only relies on the
       returned pointer, not on that numeric mapped address. */
    return data;
}
