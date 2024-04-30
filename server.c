/******************************************************************************
* myServer.c
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
#include <signal.h>

#include "networks.h"
#include "safeUtil.h"
#include "pollLib.h"

#include "senrec.h"
#include "common.h"
#include "handleTable.h"

#define DEBUG_FLAG 1


struct ServerInfo {
	int mainServerSocket;
	int portNumber;
	int currClientSocket;
};

/**
 * Check args and return port number
*/
int checkArgs(int argc, char *argv[]) {
	int portNumber = 0;

	if (argc > 2){
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2) {
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}

/**
 * Setup server
*/
void serverSetup(struct ServerInfo *serverInfoPtr, int argc, char **argv) {

	serverInfoPtr->portNumber = checkArgs(argc, argv);
	serverInfoPtr->mainServerSocket = tcpServerSetup(serverInfoPtr->portNumber);

	setupPollSet();
	addToPollSet(serverInfoPtr->mainServerSocket);

}

/**
 * Cleanup server
*/
void serverTeardown(const struct ServerInfo *serverInfoPtr) {

	close(serverInfoPtr->mainServerSocket);
	cleanupTable();
}

/**
 * Tell client bad connection
*/
void sendClientInitError(int clientSocket) {
	sendHeaderOnly(clientSocket, 3);
}

/**
 * Tell client good connection
*/
void sendClientInitGood(int clientSocket) {
	sendHeaderOnly(clientSocket, 2);
}

/**
 * Add new client
*/
void addClient(struct ServerInfo *serverInfoPtr) {
	uint8_t dataBuffer[MAX_PACKET_SIZE];
	char newHandle[MAX_HANDLE_SIZE];
	uint8_t newHandleLen = 0;
	int clientSocket;

	clientSocket = tcpAccept(serverInfoPtr->mainServerSocket, DEBUG_FLAG);

	addToPollSet(clientSocket);
	clientSocket = pollCall(-1);	// Should be from clientSocket
	recvPDU(clientSocket, dataBuffer, MAX_PACKET_SIZE);
	newHandleLen = dataBuffer[FLAG_SIZE];
	memcpy(newHandle, (char *)(dataBuffer + FLAG_SIZE + HANDLE_LEN_SIZE), newHandleLen);
	newHandle[newHandleLen] = '\0';

	if(dataBuffer[0] != 1 || handleInUse(newHandle)) {
		removeFromPollSet(clientSocket);
		sendClientInitError(clientSocket);
		close(clientSocket);
	} else {
		insertEntry(newHandle, clientSocket);
		sendClientInitGood(clientSocket);
	}
}

/**
 * Send to sending client that a given handle doesn't exist
*/
void sendNoHandle(struct HandleInfo *handleInfoPtr, const struct ServerInfo *serverInfoPtr) {
	uint8_t packetBuffer[MAX_PACKET_SIZE];
	int packetLen = 0;

	packetBuffer[0] = 7;	// Flag
	packetBuffer[1] = handleInfoPtr->handleLen;
	memcpy(packetBuffer + 2, (uint8_t *)handleInfoPtr->handle, handleInfoPtr->handleLen);
	packetLen = 2 + handleInfoPtr->handleLen;

	sendPDU(serverInfoPtr->currClientSocket, packetBuffer, packetLen);
}

/**
 * Send message to client
*/
void sendMessage(int destSocket, struct MulticastPacketInfo *multicastPacketInfoPtr) {
	sendPDU(destSocket, multicastPacketInfoPtr->packetBuffer, multicastPacketInfoPtr->packetLen);
}

/**
 * Forward multicast/message. Send errors to source if needed.
*/
void forwardMulticast(struct MulticastPacketInfo *multPacketInfoPtr, const struct ServerInfo *serverInfoPtr) {
	struct HandleInfo *handleInfoList = multPacketInfoPtr->handleInfoList;
	int currSocNum = 0;

	for(int i = 0; i < multPacketInfoPtr->numDestHandles; i++) {
		currSocNum = getSocketNum(handleInfoList[i].handle);
		if(currSocNum == -1) {
			sendNoHandle(&handleInfoList[i], serverInfoPtr);
		} else {
			sendMessage(currSocNum, multPacketInfoPtr);
		}
	}
}

/**
 * Forward broadcast
*/
void forwardBroadcast(struct MulticastPacketInfo *bcstPacketInfoPtr, const struct ServerInfo *serverInfoPtr) {
	struct GlobalEntry globalEntry;
	int destSocket;

	while(getNextHandle(&globalEntry) != -1) {
		destSocket = globalEntry.socketNum;

		if(destSocket != serverInfoPtr->currClientSocket) {
			sendMessage(destSocket, bcstPacketInfoPtr);
		}
	}
}

/**
 * Handle multicast/msg/broadcast
*/
void handleMulticast(const struct ServerInfo *serverInfoPtr, uint8_t *incomPacket, int incomPacketSize) {
	struct MulticastPacketInfo multPacketInfo;

	multPacketInfo.packetLen = incomPacketSize;
	multPacketInfo.packetBuffer = incomPacket;
	populatePacketInfo(&multPacketInfo);

	if(multPacketInfo.flag != 4) {
		forwardMulticast(&multPacketInfo, serverInfoPtr);
	} else {
		forwardBroadcast(&multPacketInfo, serverInfoPtr);
	}
}

/**
 * Handle client exit request
*/
void handleExit(const struct ServerInfo *serverInfoPtr) {
	
	sendHeaderOnly(serverInfoPtr->currClientSocket, 9);
	socRemoveFromTable(serverInfoPtr->currClientSocket);
	removeFromPollSet(serverInfoPtr->currClientSocket);
	close(serverInfoPtr->currClientSocket);
}

/**
 * Send num handles packet to client
*/
void sendNumHandles(const struct ServerInfo *serverInfoPtr, int numHandles) {
	uint8_t packetBuffer[F11_PKT_SIZE];

	packetBuffer[0] = 11;
	*(uint32_t *)(packetBuffer + FLAG_SIZE) = htonl(numHandles);

	sendPDU(serverInfoPtr->currClientSocket, packetBuffer, F11_PKT_SIZE);
}

/**
 * Send handle info to client for listing request
*/
void sendHandleInfo(const struct ServerInfo *serverInfoPtr, const struct GlobalEntry *gEntryPtr) {
	uint8_t packetBuffer[MAX_PACKET_SIZE];
	uint8_t handleLen;

	handleLen = strlen(gEntryPtr->handle);
	
	packetBuffer[0] = 12; // Flag
	packetBuffer[1] = handleLen;
	memcpy(packetBuffer + 2, (uint8_t *)(gEntryPtr->handle), handleLen);
	sendPDU(serverInfoPtr->currClientSocket, packetBuffer, handleLen + 2);
}

/**
 * Send final %L packet
*/
void sendListingEnd(const struct ServerInfo *serverInfoPtr) {
	sendHeaderOnly(serverInfoPtr->currClientSocket, 13);
}

/**
 * Handle client request for handle listing
*/
void handleListing(const struct ServerInfo *serverInfoPtr) {
	int numHandles = 0;
	struct GlobalEntry globalEntry;

	numHandles = getNumHandles();
	sendNumHandles(serverInfoPtr, numHandles);

	while(getNextHandle(&globalEntry) != -1) {
		sendHandleInfo(serverInfoPtr, &globalEntry);
	}

	sendListingEnd(serverInfoPtr);
}

/**
 * Handle client 
*/
void handleClient(const struct ServerInfo *serverInfoPtr) {
	uint8_t incomPacket[MAX_PACKET_SIZE];
	int incomPacketSize;
	uint8_t flag = 0;

	incomPacketSize = recvPDU(serverInfoPtr->currClientSocket, incomPacket, MAX_PACKET_SIZE);
	if(incomPacketSize == 0) {
		socRemoveFromTable(serverInfoPtr->currClientSocket);
		removeFromPollSet(serverInfoPtr->currClientSocket);
		close(serverInfoPtr->currClientSocket);
		return;
	}

	flag = incomPacket[0];

	switch(flag) {
		case 4:
		case 5:
		case 6:	
			handleMulticast(serverInfoPtr, incomPacket, incomPacketSize);
			break;
		case 8:
			handleExit(serverInfoPtr);
			break;
		case 10:
			handleListing(serverInfoPtr);
			break;
		default:
			fprintf(stderr, "Server: Unknown flag\n");
	}
}

/**
 * Control server
*/
void serverControl(struct ServerInfo *serverInfoPtr) {
	int pollSocket = 0;

	while(1) {
		pollSocket = pollCall(-1);
		serverInfoPtr->currClientSocket = pollSocket;
		if(pollSocket == serverInfoPtr->mainServerSocket) {
			addClient(serverInfoPtr);
		} else {
			handleClient(serverInfoPtr);
		}
	}
}

int main(int argc, char **argv) {
	struct ServerInfo serverInfo;

	serverSetup(&serverInfo, argc, argv);
	serverControl(&serverInfo);
	serverTeardown(&serverInfo);

	return 0;
}


