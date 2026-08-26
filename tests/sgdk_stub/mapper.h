#ifndef MAPPER_H
#define MAPPER_H
#include <stdbool.h>
#include <stdint.h>
typedef uint16_t u16;
u16 SYS_getBank(u16 regionIndex);
void SYS_setBank(u16 regionIndex, u16 bankIndex);
void* SYS_getFarDataEx(void* data, bool high);
#endif
