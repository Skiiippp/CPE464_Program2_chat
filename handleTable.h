
#ifndef HANDLETABLE_H
#define HANDLETABLE_H

void insertEntry(const char *handle, int socketNum);
char *getHandle(int socketNum);
int getSocketNum(const char *handle);
void cleanupTable();

#endif