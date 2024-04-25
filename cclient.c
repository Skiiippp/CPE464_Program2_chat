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
	int socketNum;
	char handle[MAX_HANDLE_SIZE];	
	uint8_t handleLen;
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
 * Control client
*/
void clientControl(struct ClientInfo *clientInfoPtr) {

	while(1) {

	}
}

int main(int argc, char **argv) {
	struct ClientInfo clientInfo;

	clientSetup(&clientInfo, argc, argv);
	clientControl(&clientInfo);
	clientTeardown(&clientInfo);
	
	return 0;
}

