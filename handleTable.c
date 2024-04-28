#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "handleTable.h"

#define ENTRY_SIZE sizeof(struct TableEntry)
#define HANDLE_SIZE 100 

#define VALID 1
#define INVALID 0

// Globals
static struct TableEntry *serverHandleTable;
static int tableSize = 0;

struct TableEntry {
    char handle[HANDLE_SIZE];
    int socketNum;
    uint8_t isValid;
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
    strncpy(newEntryPtr->handle, handle, HANDLE_SIZE);
    newEntryPtr->socketNum = socketNum;
    newEntryPtr->isValid = VALID;
}

/**
 * Remove an entry from the table based on socket num   
*/
void socRemoveFromTable(int socketNum) {
    for(int i = 0; i < tableSize; i++) {
        if(serverHandleTable[i].socketNum == socketNum && serverHandleTable[i].isValid == VALID) {
            serverHandleTable[i].isValid = INVALID;
            return;
        }
    }
    fprintf(stderr, "Couldn't find table entry to remove\n");
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
        if(serverHandleTable[i].socketNum == socketNum && serverHandleTable[i].isValid == VALID) {
            return serverHandleTable[i].handle;
        }
    }

    return NULL;
}

/**
 * Check if handle in use
 * Ret 0 if not in use
*/
int handleInUse(const char *handle) {


    if(tableSize == 0 || getSocketNum(handle) == -1) {
        return 0;
    }
    
    return 1;
}

/**
 * Returns socketNum based on handle
 * Return -1 if not found
*/
int getSocketNum(const char *handle) {

    for(int i = 0; i < tableSize; i++){
        if(strncmp(serverHandleTable[i].handle, handle, HANDLE_SIZE) == 0 && serverHandleTable->isValid == VALID) {
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