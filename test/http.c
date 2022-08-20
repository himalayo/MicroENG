#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
main(void)
{
	char lol[20];
	sprintf(lol,"%d",*(int*)"GET ");
	printf(lol);
}
