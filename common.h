
#ifndef COMMON_H
#define COMMON_H

/**
 * 3 byte header, 1 byte length of client handle, 99 bytes for client handle name,
 * 1 byte for # dest. clients (max 9 for %C), (1 byte for len of dest handle, 99 bytes
 *  for dest handle name) * 9, 200 bytes for text message = 1204 bytes
 * 
*/
#define MAX_PACKET_SIZE 1204
#define PACKET_LEN_SIZE 2

#endif