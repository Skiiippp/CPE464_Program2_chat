#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "handleTable.h"

#define ENTRY_SIZE sizeof(struct TableEntry)
#define HANDLE_SIZE 100 

// Globals
static struct TableEntry *serverHandleTable;
static int tableSize = 0;

struct TableEntry {
    char handle[HANDLE_SIZE];
    int socketNum;
} __attribute__((packed));

/**
 * Increase table size
*/
static void expandTable() {

    tableSize++;
    serverHandleTable = realloc(serverHandleTable, ENTRY_SIZE * tableSize);
}

/**
 * Fill empty entry - assues new empty entry alloced
*/
static void fillNewEntry(const char *handle, int socketNum) {
    struct TableEntry *newEntryPtr;

    newEntryPtr = &serverHandleTable[tableSize - 1];
    strncpy(newEntryPtr->handle, handle, ENTRY_SIZE);

    newEntryPtr->socketNum = socketNum;
}


/**
 * Append an entry to end of table
*/
void insertEntry(const char *handle, int socketNum) {

    if(strlen(handle) >= HANDLE_SIZE) {
        fprintf(stderr, "Error: handle greater than char limit\n");
        return;
    }

    expandTable();
    fillNewEntry(handle, socketNum);
}

/**
 * Returns handle based on socketNum
 * Return NULL if not found
*/
char *getHandle(int socketNum) {

    for(int i = 0; i < tableSize; i++) {
        if(serverHandleTable[i].socketNum == socketNum) {
            return serverHandleTable[i].handle;
        }
    }

    return NULL;
}

/**
 * Returns socketNum based on handle
 * Return -1 if not found
*/
int getSocketNum(const char *handle) {

    for(int i = 0; i < tableSize; i++){
        if(strncmp(serverHandleTable[i].handle, handle, HANDLE_SIZE) == 0) {
            return serverHandleTable[i].socketNum;
        }
    }

    return -1;
}

/**
 * Free table entries
*/
void cleanupTable() {

    free(serverHandleTable);

    tableSize = 0;
}