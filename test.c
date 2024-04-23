#include <stdio.h>

#include "handleTable.h"

int main() {

    insertEntry("BALLSFARTED", 69);
    insertEntry("IMFUCKINGGAY", 420);
    insertEntry("SHITASS", 4);

    printf("Handle: %s, Socket: %d\n", getHandle(69), getSocketNum("BALLSFARTED"));
    printf("Handle: %s, Socket: %d\n", getHandle(420), getSocketNum("IMFUCKINGGAY"));
    printf("Handle: %s, Socket: %d\n", getHandle(4), getSocketNum("SHITASS"));

    if(getSocketNum("IMTRANS") == -1) {
        printf("Good handle err\n");
    }

    if(getHandle(12) == NULL) {
        printf("Good socket err\n");
    }

    cleanupTable();
    
    return 0;
}