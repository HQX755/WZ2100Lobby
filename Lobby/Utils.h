#ifndef _UTILS_H_
#define _UTILS_H_

#include "stdafx.h"

static inline size_t strlcpy(char *dest, const char *src, size_t size)
{
	if (size > 0)
	{
		strncpy(dest, src, size - 1);
		dest[size - 1] = '\0';
	}

	return strlen(src);

}

#define sstrcpy(dest, src) (strlcpy((dest), (src), sizeof(dest)))

unsigned long GetRunTime();

#endif