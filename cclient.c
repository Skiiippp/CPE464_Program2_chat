/******************************************************************************
* myClient.c
*
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
*
* Updated by James Gruber April 2024
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

// Function prototypes for main control functions
void clientSetup(struct ClientInfo *clientInfoPtr, int argc, char **argv);
void clientControl(struct ClientInfo *clientInfoPtr);
void clientTeardown(struct ClientInfo *clientInfoPtr);

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
				fprintf(stderr, "\nServer Terminated\n");
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
 * Input buffer should start at point where handle names begin,
 * or message for broadcast
 * Only fills packet len from flag to message
*/
void fillMultHandles(struct MulticastPacketInfo *multPktInfoPtr, uint8_t *inputBuffer) {
	int numHandles;
	struct HandleInfo *infoList;
	uint8_t handleLen;
	uint8_t *currentBuff = inputBuffer;
	int packetLen = multPktInfoPtr->senderInfo.handleLen + 3; // flag, sending size, # headers

	if(multPktInfoPtr->flag != 4) {
		packetLen = multPktInfoPtr->senderInfo.handleLen + 3;	// Not broadcast
	} else {
		packetLen = multPktInfoPtr->senderInfo.handleLen + 2; // No num handles
	}

	numHandles = multPktInfoPtr->numDestHandles;
	infoList = multPktInfoPtr->handleInfoList;
	for(int i = 0; i < numHandles; i++) {
		handleLen = getStrLen((char *)currentBuff);
		packetLen += (1 + handleLen);	// 1 for handle size
		infoList[i].handleLen = handleLen;
		memcpy(infoList[i].handle, currentBuff, handleLen);
		infoList[i].handle[handleLen] = '\0';	// set null terminator
		currentBuff += handleLen + 1;	// 1 for space
	}
	multPktInfoPtr->packetLen = packetLen;	// Not including message len

	multPktInfoPtr->message = (char *)currentBuff;
	multPktInfoPtr->messageLen = strlen(multPktInfoPtr->message) + 1;	// 1 for null
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

	if(multPktInfoPtr->flag != 4) {	// Not broadcast
		currPacketBuff[0] = multPktInfoPtr->numDestHandles;
		currPacketBuff += 1;
	}

	for(int i = 0; i < multPktInfoPtr->numDestHandles; i++) {
		currDestInfoPtr = &(multPktInfoPtr->handleInfoList[i]);
		*currPacketBuff = currDestInfoPtr->handleLen;
		currPacketBuff += 1;
		memcpy(currPacketBuff, currDestInfoPtr->handle, currDestInfoPtr->handleLen);
		currPacketBuff += currDestInfoPtr->handleLen;
	}

	strncpy((char *)currPacketBuff, multPktInfoPtr->message, MAX_MSG_SIZE);
}

/**
 * Do final processing and send a message/multicast/broadcast packet
*/
void offSend(const struct ClientInfo *clientInfoPtr, struct MulticastPacketInfo *multPktInfoPtr) {
	int numPackets = 0; // Depends on number of messages needed
	struct MulticastPacketInfo tempPktInfo;
	uint8_t *packetBuffer;
	char messageBuffer[MAX_MSG_SIZE];
	char *currPktMessage = multPktInfoPtr->message;	// increment as needed
	int currMessageLen = 0;

	packetBuffer = multPktInfoPtr->packetBuffer;
	memcpy(&tempPktInfo, multPktInfoPtr, sizeof(struct MulticastPacketInfo));	// Used for packing packet
	currMessageLen = strlen(multPktInfoPtr->message) + 1;	// 1 for null
	numPackets = (int)(currMessageLen/MAX_MSG_SIZE) + 1;
	for(int i = 0; i < numPackets; i++) {
		strncpy(messageBuffer, currPktMessage, MAX_MSG_SIZE);
		messageBuffer[MAX_MSG_SIZE - 1] = '\0';
		currMessageLen = strlen(messageBuffer) + 1; // for null
		tempPktInfo.message = messageBuffer;
		tempPktInfo.packetLen += currMessageLen;
		packMultPacket(&tempPktInfo, packetBuffer + FLAG_SIZE);
		sendPDU(clientInfoPtr->socketNum, packetBuffer, tempPktInfo.packetLen);

		tempPktInfo.packetLen -= currMessageLen;
		currPktMessage += MAX_MSG_SIZE - 1;
	}
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
	msgPktInfo.flag = 5;
	msgPktInfo.packetBuffer = packetBuffer;
	fillMultHandles(&msgPktInfo, inputBuffer + MSG_INPUT_OFFSET_TO_HANDLE);
	offSend(clientInfoPtr, &msgPktInfo);
} 

/**
 * Broadcast a message to all clients except self
*/
void sendBroadcast(struct ClientInfo *clientInfoPtr, uint8_t *inputBuffer) {
	struct MulticastPacketInfo bcstPktInfo;
	uint8_t packetBuffer[MAX_PACKET_SIZE];

	bcstPktInfo.senderInfo.handleLen = clientInfoPtr->handleLen;
	strncpy(bcstPktInfo.senderInfo.handle, clientInfoPtr->handle, MAX_HANDLE_SIZE);
	bcstPktInfo.numDestHandles = 0;	// Not actually "true", but just in packet no dest. handles

	packetBuffer[0] = 4;
	bcstPktInfo.flag = 4;
	bcstPktInfo.packetBuffer = packetBuffer;
	fillMultHandles(&bcstPktInfo, inputBuffer + BROADCAST_OFFSET_TO_MSG);
	offSend(clientInfoPtr, &bcstPktInfo);
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

	if(multPktInfo.numDestHandles < 2 || multPktInfo.numDestHandles > 9) {
		fprintf(stderr, "Invalid number of destination handles for multicast\n");
	}

	packetBuffer[0] = 6; // flag
	multPktInfo.flag = 6;
	multPktInfo.packetBuffer = packetBuffer;
	fillMultHandles(&multPktInfo, inputBuffer + MULT_INPUT_OFFSET_TO_HANDLES);
	offSend(clientInfoPtr, &multPktInfo);
}

/**
 * Ask server for client listing
*/
void sendListing(struct ClientInfo *clientInfoPtr) {
	sendHeaderOnly(clientInfoPtr->socketNum, 10);
}

/**
 * Request disconnect from server - NEED TO COMPLETE
*/
void sendExit(struct ClientInfo *clientInfoPtr) {
	uint8_t packetBuffer[FLAG_SIZE];
	
	packetBuffer[0] = 8;
	sendPDU(clientInfoPtr->socketNum, packetBuffer, FLAG_SIZE);
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
			sendListing(clientInfoPtr);
			break;
		case 'E':
		case 'e': 
			sendExit(clientInfoPtr);
			break;
		default:
			printf("Invalid command\n");
	}

}


/**
 * Server couldn't find handle
*/
void recvHandleNotFound(uint8_t *packetBuffer) {
	struct HandleInfo handleInfo;

	popHandleInfo(&handleInfo, packetBuffer + FLAG_SIZE);
	fprintf(stderr, "\nClient with handle %s does not exist\n", handleInfo.handle);
}

/**
 * Recieve multicast/msg/broadcast
*/
void recvMulticast(uint8_t *packetBuffer, int packetLen) {
	struct MulticastPacketInfo multicastPacketInfo;

	multicastPacketInfo.packetLen = packetLen;
	multicastPacketInfo.packetBuffer = packetBuffer;
	populatePacketInfo(&multicastPacketInfo);

	printf("\n%s: %s\n", multicastPacketInfo.senderInfo.handle, multicastPacketInfo.message);
}

/**
 * Populate the next listing packet, returning incoming flag. Dont proc stdin
*/
uint8_t getNextListingPacket(struct ClientInfo *clientInfoPtr, uint8_t *packetBuffer) {
	
	while(pollCall(-1) != clientInfoPtr->socketNum);
	recvPDU(clientInfoPtr->socketNum, packetBuffer, MAX_PACKET_SIZE);
	return packetBuffer[0];
}

/**
 * Recieve listing. Read flag 12 packets (blocking) until flag 13 packet
*/
void recvListing(struct ClientInfo *clientInfoPtr, uint8_t *firstPacketBuffer) {
	uint8_t packetBuffer[MAX_PACKET_SIZE];
	int numHandles = 0;
	uint8_t incomingFlag = 12;
	struct HandleInfo handleInfo;

	numHandles = ntohl(*(uint32_t *)(firstPacketBuffer + FLAG_SIZE));
	printf("\nNumber of clients: %d\n", numHandles);

	// Get first handle packet, dont process stdin
	incomingFlag = getNextListingPacket(clientInfoPtr, packetBuffer);
	while(incomingFlag == 12) {

		popHandleInfo(&handleInfo, packetBuffer + FLAG_SIZE);
		printf("\t%s\n", handleInfo.handle);
		incomingFlag = getNextListingPacket(clientInfoPtr, packetBuffer);
	}
}

/**
 * Process message from server
*/
void processMsgFromServer(struct ClientInfo *clientInfoPtr, uint8_t *exitFlagPtr) {
	uint8_t dataBuffer[MAX_PACKET_SIZE];
	int inputSize;
	uint8_t flag;

	inputSize = recvPDU(clientInfoPtr->socketNum, dataBuffer, MAX_PACKET_SIZE);
	if(inputSize == 0) {
		fprintf(stderr, "Server Terminated\n");
		clientTeardown(clientInfoPtr);
		exit(-1);
	} 

	flag = dataBuffer[0];
	switch(flag) {
		case 4:
		case 5:
		case 6:
			recvMulticast(dataBuffer, inputSize);
			break;
		case 7:
			recvHandleNotFound(dataBuffer);
			break;
		case 9:
			*exitFlagPtr = 1;
			printf("\n");
			break;
		case 11:
			recvListing(clientInfoPtr, dataBuffer);
			break;
		default:
			fprintf(stderr, "Unknown flag\n");
	}
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
 * Control client
*/
void clientControl(struct ClientInfo *clientInfoPtr) {
	int socketInUse = 0;
	uint8_t exitFlag = 0;

	printf("$: ");
	fflush(stdout);

	while(!exitFlag) {
		socketInUse = pollCall(-1);
		if(socketInUse == STDIN_FILENO) {
			processInput(clientInfoPtr);
		} else {
			processMsgFromServer(clientInfoPtr, &exitFlag);
		}

		printf("$: ");
		fflush(stdout);
	}
}

/**
 * Teardown client
*/
void clientTeardown(struct ClientInfo *clientInfoPtr) {
	
	removeFromPollSet(clientInfoPtr->socketNum);
	close(clientInfoPtr->socketNum);
}


int main(int argc, char **argv) {
	struct ClientInfo clientInfo;

	clientSetup(&clientInfo, argc, argv);
	clientControl(&clientInfo);
	clientTeardown(&clientInfo);	
	
	printf("\n");

	return 0;
}

