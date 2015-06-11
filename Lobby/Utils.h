#ifndef _UTILS_H_
#define _UTILS_H_

#include "stdafx.h"
#include "crc.h"

static inline size_t strlcpy(char *dest, const char *src, size_t size)
{
	if (size > 0)
	{
		strncpy(dest, src, size - 1);
		dest[size - 1] = '\0';
	}

	return strlen(src);

}

template<typename T>
T *Memcpy(T *src, size_t size)
{
	T* dst = new T[size];
	std::copy(src, src + size, dst);

	return dst;
}

template<typename SRC, typename DST>
void __CopyMemory(DST *dst, SRC *src, size_t size)
{
	std::copy(reinterpret_cast<char*>(src), reinterpret_cast<char*>(reinterpret_cast<size_t>(src) + size), reinterpret_cast<char*>(dst));
}

#define sstrcpy(dest, src) (strlcpy((dest), (src), sizeof(dest)))
#define mmemcpy(dest, src, size) __CopyMemory(dest, src, size)
#define c_string(str) std::string(str).c_str()
#define	HEADER_OF(p) *(uint16_t*)p

unsigned long GetRunTime();

#endif