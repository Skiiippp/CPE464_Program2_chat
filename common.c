#include <string.h>
    
#include "common.h"
#include "stddef.h"
#include "senrec.h"

/**
 * Return length of handle - expects current
 * pointer to be start of string
*/
int getStrLen(char *inputStr) {
	
	for(int i = 0; i < MAX_HANDLE_SIZE; i++) {
		if(inputStr[i] == ' ' || inputStr[i] == 0) {
            return i;
        }
	}
    return MAX_HANDLE_SIZE;
}

/**
 * Given packet buffer pointing to handle length, populate a handle info struct
*/
void popHandleInfo(struct HandleInfo *handleInfoPtr, uint8_t *packetBuffer) {

    handleInfoPtr->handleLen = packetBuffer[0];
    memcpy(handleInfoPtr->handle, packetBuffer + 1, handleInfoPtr->handleLen);
    handleInfoPtr->handle[handleInfoPtr->handleLen] = '\0';    // Null terminator
}

/**
 * Populate the fields of multicast packet struct from a packet buffer
 * Length and packet fields of struct should already be populated
*/
void populatePacketInfo(struct MulticastPacketInfo *multPacketInfoPtr) {
	uint8_t *currPacketBuffer;

	currPacketBuffer = multPacketInfoPtr->packetBuffer + FLAG_SIZE;

	popHandleInfo(&(multPacketInfoPtr->senderInfo), currPacketBuffer);
	currPacketBuffer += multPacketInfoPtr->senderInfo.handleLen + 1;	// 1 for handle size field

	multPacketInfoPtr->numDestHandles = currPacketBuffer[0];
	currPacketBuffer += 1;	// Now pointing to len of first dest handle

	for(int i = 0; i < multPacketInfoPtr->numDestHandles; i++) {
		popHandleInfo(&(multPacketInfoPtr->handleInfoList[i]), currPacketBuffer);
		currPacketBuffer += multPacketInfoPtr->handleInfoList[i].handleLen + 1;
	}

	multPacketInfoPtr->message = (char *)currPacketBuffer;
}

/**
 * Send a packet that just contains chat header
*/
void sendHeaderOnly(int targetSocket, uint8_t flag) {
	uint8_t packetBuffer[FLAG_SIZE];

	packetBuffer[0] = flag;
	sendPDU(targetSocket, packetBuffer, FLAG_SIZE);
}