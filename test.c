#include <stdio.h>

#include "handleTable.h"



int main() {


    insertEntry("balls", 420);

    printf("%d\n",handleInUse("balls"));

    socRemoveFromTable(420);

    printf("%d\n",handleInUse("balls"));

    return 0;
}