/******************************************************************************
* myClient.c
*
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
* Updated by James Gruber April
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>

#include "networks.h"
#include "safeUtil.h"
#include "pollLib.h"

#include "senrec.h"
#include "common.h"

#define DEBUG_FLAG 1

struct ClientInfo {
	int socketNum;	// to server
	char handle[MAX_HANDLE_SIZE];	
	uint8_t handleLen;	// No NULL
};

void checkArgs(int argc, char **argv) {

	if (argc != 4) {
		fprintf(stderr, "usage: %s handle host-name port-number \n", argv[0]);
		exit(-1);
	}

	if(strlen(argv[1]) > MAX_HANDLE_SIZE) {
		fprintf(stderr, "Invalid handle, handle longer than 100 characters: %s\n", argv[1]);
		exit(-1);
	}
}

/**
 * Wait for initial resp from server
*/
void waitForInitResp(struct ClientInfo *clientInfoPtr) {
	int bytesRecieved = 0;
	uint8_t contFlag = 0;
	uint8_t dataBuffer[FLAG_SIZE];	// Just flag

	while(!contFlag) {

		if(pollCall(-1) == clientInfoPtr->socketNum) {
			
			bytesRecieved = recvPDU(clientInfoPtr->socketNum, dataBuffer, FLAG_SIZE);
			if(bytesRecieved == 0) {
				fprintf(stderr, "Server Terminated\n");
				exit(-1);
			} else if(bytesRecieved == -1) {
				fprintf(stderr, "Unknown error occured on initial client recv call\n");
				exit(-1);
			}

			if(dataBuffer[0] == 2) {
				contFlag = 1;	// Successful connection established
			} else if(dataBuffer[0] == 3) {
				fprintf(stderr, "Handle already in use: %s\n", clientInfoPtr->handle);
				exit(-1);				
			}
		}
	}
}

/**
 * Sends initial logon packet to server
*/
void sendInitialPacket(struct ClientInfo *clientInfoPtr) {
	int lengthOfData = 0;
	uint8_t dataBuffer[MAX_PACKET_SIZE];

	lengthOfData = FLAG_SIZE + HANDLE_LEN_SIZE + clientInfoPtr->handleLen;
	
	dataBuffer[0] = 1;	// Flag
	dataBuffer[1] = clientInfoPtr->handleLen;
	memcpy(dataBuffer + 2, clientInfoPtr->handle, clientInfoPtr->handleLen);

	sendPDU(clientInfoPtr->socketNum, dataBuffer, lengthOfData);
}

void establishConnection(struct ClientInfo *clientInfoPtr) {

	sendInitialPacket(clientInfoPtr);
	waitForInitResp(clientInfoPtr);
}

/**
 * Setup client
*/
void clientSetup(struct ClientInfo *clientInfoPtr, int argc, char **argv) {

	checkArgs(argc, argv);

	clientInfoPtr->socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
	clientInfoPtr->handleLen = strlen(argv[1]);
	strncpy(clientInfoPtr->handle, argv[1], MAX_HANDLE_SIZE);
	clientInfoPtr->handleLen = strlen(clientInfoPtr->handle);

	setupPollSet();
	addToPollSet(clientInfoPtr->socketNum);
	addToPollSet(STDIN_FILENO);

	establishConnection(clientInfoPtr);
}

/**
 * Teardown client
*/
void clientTeardown(struct ClientInfo *clientInfoPtr) {
	
	removeFromPollSet(clientInfoPtr->socketNum);
	close(clientInfoPtr->socketNum);
}

/**
 * Read stdin
*/
int readFromStdin(uint8_t * buffer) {
	char aChar = 0;
	int inputLen = 0;        

	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	while (inputLen < (MAX_INPUT_SIZE - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}

	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;

	return inputLen;
}

/**
 * Populate handle info array in multicast info struct
 * Input buffer should start at point where handle names begin
*/
void fillMultHandles(struct MulticastPacketInfo *multPktInfoPtr, uint8_t *inputBuffer) {
	int numHandles;
	struct HandleInfo *infoList;
	uint8_t handleLen;
	uint8_t *currentBuff = inputBuffer;

	numHandles = multPktInfoPtr->numDestHandles;
	infoList = multPktInfoPtr->handleInfoList;
	for(int i = 0; i < numHandles; i++) {
		handleLen = getHandleLen((char *)currentBuff);
		infoList[i].handleLen = handleLen;
		memcpy(infoList[i].handle, currentBuff, handleLen);
		infoList[i].handle[handleLen] = '\0';	// set null terminator
		currentBuff += handleLen + 1;	// 1 for space
	}

	multPktInfoPtr->message = (char *)currentBuff;
}

/**
 * Pack message/multicast packet. Buffer should point to byte after flag 
*/
void packMultPacket(struct MulticastPacketInfo *multPktInfoPtr, uint8_t *packetBuffer) {
	struct HandleInfo *senderInfoPtr = &(multPktInfoPtr->senderInfo);
	struct HandleInfo *currDestInfoPtr;
	uint8_t *currPacketBuff = packetBuffer;

	*currPacketBuff = senderInfoPtr->handleLen;
	currPacketBuff += 1;
	memcpy(currPacketBuff, senderInfoPtr->handle, senderInfoPtr->handleLen);
	currPacketBuff += senderInfoPtr->handleLen;

	for(int i = 0; i < multPktInfoPtr->numDestHandles; i++) {
		currDestInfoPtr = &(multPktInfoPtr->handleInfoList[i]);
		*currPacketBuff = currDestInfoPtr->handleLen;
		currPacketBuff += 1;
		memcpy(currPacketBuff, currDestInfoPtr->handle, currDestInfoPtr->handleLen);
		currPacketBuff += currDestInfoPtr->handleLen;
	}

	strncpy((char *)currPacketBuff, multPktInfoPtr->message, MAX_TEXT_SIZE); 
}

/**
 * Send a message to a client
*/
void sendMessage(struct ClientInfo *clientInfoPtr, uint8_t *inputBuffer) {
	struct MulticastPacketInfo msgPktInfo;
	uint8_t packetBuffer[MAX_PACKET_SIZE];

	msgPktInfo.senderInfo.handleLen = clientInfoPtr->handleLen;
	strncpy(msgPktInfo.senderInfo.handle, clientInfoPtr->handle, MAX_HANDLE_SIZE);
	msgPktInfo.numDestHandles = 1;

	packetBuffer[0] = 5;	// flag
	fillMultHandles(&msgPktInfo, inputBuffer + MSG_INPUT_OFFSET_TO_HANDLE);
	packMultPacket(&msgPktInfo, packetBuffer + FLAG_SIZE);
} 

/**
 * Broadcast a message to all clients except self
*/
void sendBroadcast(struct ClientInfo *clientInfoPtr, uint8_t *inputBuffer) {

}

/**
 * Send a message to specified clients
*/
void sendMulticast(struct ClientInfo *clientInfoPtr, uint8_t *inputBuffer) {
	struct MulticastPacketInfo multPktInfo;
	uint8_t packetBuffer[MAX_PACKET_SIZE];

	multPktInfo.senderInfo.handleLen = clientInfoPtr->handleLen;
	strncpy(multPktInfo.senderInfo.handle, clientInfoPtr->handle, MAX_HANDLE_SIZE);
	multPktInfo.numDestHandles = atoi((char *)(inputBuffer + MULT_INPUT_OFFSET_TO_NUM_HANDLES));

	fillMultHandles(&multPktInfo, inputBuffer + MULT_INPUT_OFFSET_TO_HANDLES);
	packetBuffer[0] = 6; // flag
	packMultPacket(&multPktInfo, packetBuffer + FLAG_SIZE);
}

/**
 * Ask server for client listing
*/
void sendListing(struct ClientInfo *clientInfoPtr, uint8_t *inputBuffer) {

}

/**
 * Request disconnect from server - NEED TO COMPLETE
*/
void sendExit(struct ClientInfo *clientInfoPtr, uint8_t *inputBuffer) {

	close(clientInfoPtr->socketNum);
}

/**
 * Process input
*/
void processInput(struct ClientInfo *clientInfoPtr) {
	uint8_t inputBuffer[MAX_INPUT_SIZE];

	readFromStdin(inputBuffer);
	
	if(inputBuffer[0] != '%') {
		fprintf(stderr, "Invalid command format\n");
		return;
	}

	switch(inputBuffer[1]) {
		case 'M':
		case 'm':
			sendMessage(clientInfoPtr, inputBuffer);
			break;
		case 'B':
		case 'b':
			sendBroadcast(clientInfoPtr, inputBuffer);
			break;
		case 'C':
		case 'c':
			sendMulticast(clientInfoPtr, inputBuffer);
			break;
		case 'L':
		case 'l':
			sendListing(clientInfoPtr, inputBuffer);
			break;
		case 'E':
		case 'e': 
			sendListing(clientInfoPtr, inputBuffer);
			break;
		default:
			printf("Invalid command\n");
	}

}

/**
 * Process message from server
*/
void processMsgFromServer(struct ClientInfo *clientInfoPtr) {
	uint8_t dataBuffer[MAX_PACKET_SIZE];
	int inputSize;

	inputSize = recvPDU(clientInfoPtr->socketNum, dataBuffer, MAX_PACKET_SIZE);
	if(inputSize == 0) {
		fprintf(stderr, "Server Terminated\n");
		clientTeardown(clientInfoPtr);
		exit(-1);
	}
}

/**
 * Control client
*/
void clientControl(struct ClientInfo *clientInfoPtr) {
	int socketInUse = 0;

	printf("$: ");
	fflush(stdout);

	while(1) {
		socketInUse = pollCall(-1);
		if(socketInUse == STDIN_FILENO) {
			processInput(clientInfoPtr);
		} else {
			processMsgFromServer(clientInfoPtr);
		}

		printf("$: ");
		fflush(stdout);
	}
}

int main(int argc, char **argv) {
	struct ClientInfo clientInfo;

	clientSetup(&clientInfo, argc, argv);
	clientControl(&clientInfo);
	clientTeardown(&clientInfo);	// Likely unecessary
	
	return 0;
}

