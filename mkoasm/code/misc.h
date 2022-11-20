#pragma once

#define LOWORD(l)           ((uint16_t)((*(uint32_t*)(&l)) & 0xffff))
#define HIWORD(l)           ((uint16_t)(((*(uint32_t*)(&l)) >> 16) & 0xffff))

void changeEndINT(int* value);
void changeEndSHORT(short* value);