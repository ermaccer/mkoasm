#include "misc.h"

void changeEndINT(int* value)
{
	*value = (*value & 0x000000FFU) << 24 | (*value & 0x0000FF00U) << 8 | (*value & 0x00FF0000U) >> 8 | (*value & 0xFF000000U) >> 24;
}

void changeEndSHORT(short* value)
{
	*value = (*value & 0x00FFU) << 8 | (*value & 0xFF00U) >> 8;
}
