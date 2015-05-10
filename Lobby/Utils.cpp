#include "stdafx.h"
#include "Utils.h"
#include <Windows.h> //For timeval struct..

unsigned long m_dwStartTime = 0;

void _gettimeofday(struct timeval* t, struct timezone* dummy)
{
	unsigned long millisec = GetTickCount();

	t->tv_sec = (millisec / 1000);
	t->tv_usec = (millisec % 1000) * 1000;
}

float get_float_time()
{
	struct timeval tv;
	_gettimeofday(&tv, NULL);
	tv.tv_sec -= 1057699978;
	return ((float) tv.tv_sec + ((float) tv.tv_usec / 1000000.0f));
}

DWORD get_boot_sec()
{
	static struct timeval tv_boot = {0L, 0L};

	if (tv_boot.tv_sec == 0)
		_gettimeofday(&tv_boot, NULL);

	return tv_boot.tv_sec;
}

unsigned long GetRunTime()
{
	struct timeval tv;
	_gettimeofday(&tv, NULL);
	//tv.tv_sec -= 1057699978;
	tv.tv_sec -= get_boot_sec();

	return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}