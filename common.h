
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

/**
 * 3 byte header, 1 byte length of client handle, 99 bytes for client handle name,
 * 1 byte for # dest. clients (max 9 for %C), (1 byte for len of dest handle, 99 bytes
 *  for dest handle name) * 9, 200 bytes for text message = 1204 bytes
 * 
*/
#define MAX_PACKET_SIZE 1300 // ~100 greater than needed, for saftey - originally 1204
#define PACKET_LEN_SIZE 2
#define MAX_HANDLE_SIZE 100	// Counting null
#define CHAT_HEADER_SIZE 3
#define HANDLE_LEN_SIZE 1
#define FLAG_SIZE 1
#define MAX_INPUT_SIZE 1400
#define MAX_DEST_CLIENT 9 // On multicast
#define F1_HEADER_OFFSET 2 // Flag 1 packet
#define MSG_INPUT_OFFSET_TO_HANDLE 3
#define MULT_INPUT_OFFSET_TO_NUM_HANDLES 3
#define MULT_INPUT_OFFSET_TO_HANDLES 5
#define MAX_MSG_SIZE 200	//including null
#define F11_PKT_SIZE FLAG_SIZE + sizeof(int)
#define MAX_NUM_MESSAGES 7 // 1400 max input, 200 per message
#define BROADCAST_OFFSET_TO_MSG 3

// Handle and length
struct HandleInfo {
	char handle[MAX_HANDLE_SIZE];
	uint8_t handleLen;
};

// For message and multicast
struct MulticastPacketInfo {
	struct HandleInfo senderInfo;
	uint8_t numDestHandles;
	struct HandleInfo handleInfoList[MAX_DEST_CLIENT];
	char *message;
	int messageLen;	// including null
	int packetLen;	// starting at flag
	uint8_t *packetBuffer;
	uint8_t flag;
};

/**
 * Read string - return length of string term with spaces
*/
int getStrLen(char *inputStr);

/**
 * Given packet buffer pointing to handle length, populate a handle info struct
*/
void popHandleInfo(struct HandleInfo *handleInfoPtr, uint8_t *packetBuffer);

/**
 * Populate the fields of multicast packet struct from a packet buffer
 * Length and packet fields of struct should already be populated
*/
void populatePacketInfo(struct MulticastPacketInfo *multPacketInfoPtr);

/**
 * Send a packet that just contains chat header
*/
void sendHeaderOnly(int targetSocket, uint8_t flag);

#endif