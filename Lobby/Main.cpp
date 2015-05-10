#include "stdafx.h"
#include "Core.h"


int _tmain(int argc, _TCHAR* argv[])
{
	ServerInit();

	CORE()->Run();
	
	return 0;
}

