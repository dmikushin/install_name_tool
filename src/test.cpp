#include <cstdio>

extern "C" int foo()
{       
	printf("Thit is a test shared library\n");
	return 0;
}
