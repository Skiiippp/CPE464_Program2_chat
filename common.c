

#include "common.h"
#include "stddef.h"

/**
 * Return length of handle - expects current
 * pointer to be start of string
*/
int getHandleLen(char *inputStr) {
	
	for(int i = 0; i < MAX_HANDLE_SIZE; i++) {
		if(inputStr[i] == ' ' || inputStr[i] == 0) {
            return i;
        }
	}
    return MAX_HANDLE_SIZE;
}