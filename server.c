/******************************************************************************
* myServer.c
* 
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
* Additions/Mods by James Gruber, April 2024
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
	uint8_t dataBuffer[FLAG_SIZE];

	dataBuffer[0] = 3;
	sendPDU(clientSocket, dataBuffer, FLAG_SIZE);
}

/**
 * Tell client good connection
*/
void sendClientInitGood(int clientSocket) {
	uint8_t dataBuffer[FLAG_SIZE];

	dataBuffer[0] = 2;
	sendPDU(clientSocket, dataBuffer, FLAG_SIZE);
}

/**
 * Add new client
*/
void addClient(struct ServerInfo *serverInfoPtr) {
	uint8_t dataBuffer[MAX_PACKET_SIZE];
	char *newHandle;
	int clientSocket;

	clientSocket = tcpAccept(serverInfoPtr->mainServerSocket, DEBUG_FLAG);

	addToPollSet(clientSocket);
	clientSocket = pollCall(-1);	// Should be from clientSocket
	recvPDU(clientSocket, dataBuffer, MAX_PACKET_SIZE);
	newHandle = (char *)dataBuffer + F1_HEADER_OFFSET;

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
 * Control server
*/
void serverControl(struct ServerInfo *serverInfoPtr) {
	int pollSocket = 0;

	while(1) {
		
		if((pollSocket = pollCall(-1)) == serverInfoPtr->mainServerSocket) {
			addClient(serverInfoPtr);
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


