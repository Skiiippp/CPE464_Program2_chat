
#ifndef HANDLETABLE_H
#define HANDLETABLE_H

struct GlobalEntry {
    char *handle;
    int socketNum;
};

int getNextHandle(struct GlobalEntry *gEntryPtr);
int getNumHandles();
void socRemoveFromTable(int socketNum);
void insertEntry(const char *handle, int socketNum);
char *getHandle(int socketNum);
int getSocketNum(const char *handle);
int handleInUse(const char *handle);
void cleanupTable();

#endif