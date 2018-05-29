#include <stdio.h>
#include <stdlib.h>

void main() 
{
	int i = 0x12345678;
	char* pc = (char*)&i;
	if (*pc == 0x12) {
		printf("Big Endian\n");
	} else if (*pc == 0x78) {
		printf("Little Endian\n");
	}   
}