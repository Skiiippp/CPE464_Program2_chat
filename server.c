/******************************************************************************
* myServer.c
* 
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
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

#define MAXBUF 1024
#define DEBUG_FLAG 1

void serverControl(int mainServerSocket);
void processClient(int clientSocket);
int addNewSocket(int mainServerSocket, int debugFlag);
void recvFromClient(int clientSocket);
int checkArgs(int argc, char *argv[]);

int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	//int clientSocket = 0;   //socket descriptor for the client socket
	int portNumber = 0;

	setupPollSet();

	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);

	serverControl(mainServerSocket);

	close(mainServerSocket);

	
	return 0;
}

void serverControl(int mainServerSocket) {
	int pollRet = 0;

	addToPollSet(mainServerSocket);
	while(1) {
		if((pollRet = pollCall(-1)) == mainServerSocket) {
			addNewSocket(mainServerSocket, DEBUG_FLAG);
		} else {
			processClient(pollRet);
		}
	}
}

/**
 * Process client
*/
void processClient(int clientSocket) {
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	char *msg = "Message Recieved\n";

	if((messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF)) < 0) {
		fprintf(stderr, "Error: recvPDU() ret -1 on socket %d\n", clientSocket);
		return;
	}

	if(messageLen > 0) {
		printf("Message received on socket %d, length: %d Data: %s\n", clientSocket, messageLen, dataBuffer);
		sendPDU(clientSocket, (uint8_t *)msg, 18);
	} else {
		printf("Socket %d closed by other side\n", clientSocket);
		removeFromPollSet(clientSocket);
		close(clientSocket);
		
	}
}

/**
 * Return client socket
*/
int addNewSocket(int mainServerSocket, int debugFlag) {
	int clientSocket = 0;

	clientSocket = tcpAccept(mainServerSocket, debugFlag);
	addToPollSet(clientSocket);

	return clientSocket;
}

void recvFromClient(int clientSocket)
{
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;

	if((messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF)) < 0) {
		fprintf(stderr, "Error: recvPDU() ret -1\n");
		exit(-1);
	}

	if(messageLen > 0) {
		printf("Message received, length: %d Data: %s\n", messageLen, dataBuffer);
	} else {
		printf("Connection closed by other side\n");
	}
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}

