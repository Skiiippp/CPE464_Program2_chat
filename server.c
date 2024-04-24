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

static uint8_t intFlag = 0;

struct ServerInfo {
	int mainServerSocket;
	int portNumber;
};

/**
 * Check args and return port number
*/
int checkArgs(int argc, char *argv[])
{
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
 * Handle interrupt
*/
void intHandler() {
	intFlag = 1;
}

/**
 * Setup server
*/
void serverSetup(struct ServerInfo *serverInfoPtr, int argc, char **argv) {

	serverInfoPtr->portNumber = checkArgs(argc, argv);
	serverInfoPtr->mainServerSocket = tcpServerSetup(serverInfoPtr->portNumber);

	signal(SIGINT, intHandler);

	setupPollSet();
}

/**
 * Cleanup server
*/
void serverTeardown(const struct ServerInfo *serverInfoPtr) {

	close(serverInfoPtr->mainServerSocket);
	cleanupTable();
}

void serverControl(struct ServerInfo *serverInfoPtr) {
	
	while(!intFlag) {

	}
}

int main(int argc, char *argv[])
{
	struct ServerInfo serverInfo;

	serverSetup(&serverInfo, argc, argv);
	serverControl(&serverInfo);
	serverTeardown(&serverInfo);

	return 0;
}


