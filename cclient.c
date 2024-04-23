/******************************************************************************
* myClient.c
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

void processStdin(int socketNum);
void processMsgFromServer();
void clientControl(int socketNum);
void sendToServer(int socketNum);
int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);

int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor

	setupPollSet();
	
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[1], argv[2], DEBUG_FLAG);
	
	//sendToServer(socketNum);
	clientControl(socketNum);
	
	close(socketNum);
	
	return 0;
}

void processStdin(int socketNum) {
	uint8_t inBuff[MAXBUF];
	ssize_t inSize = 0;

	inSize = read(STDIN_FILENO, inBuff, MAXBUF - 1);
	inBuff[inSize - 1] = '\0';
	sendPDU(socketNum, inBuff, inSize);
}

void processMsgFromServer(int socketNum) {
	uint8_t inBuff[MAXBUF];
	ssize_t inSize = 0;

	inSize = recvPDU(socketNum, inBuff, MAXBUF);
	if(inSize == 0) {
		fprintf(stderr, "\nServer has terminated\n");
		exit(0);
	}
	printf("%s", inBuff);
}

void clientControl(int socketNum) {
	int pollRet = 0;

	addToPollSet(socketNum);
	addToPollSet(STDIN_FILENO);
	while(1) {
		printf("Enter Data: ");
		fflush(stdout);
		if((pollRet = pollCall(-1)) == STDIN_FILENO) {
			processStdin(socketNum);
		}

		if(pollRet == socketNum || pollCall(-1) == socketNum) {
			processMsgFromServer(socketNum);
		}

	}

}

void sendToServer(int socketNum)
{
	uint8_t sendBuf[MAXBUF];   //data buffer
	int sendLen = 0;        //amount of data to send
	int sent = 0;            //actual amount of data sent/* get the data and send it   */
	
	sendLen = readFromStdin(sendBuf);
	printf("read: %s string len: %d (including null)\n", sendBuf, sendLen);

	sent = sendPDU(socketNum, sendBuf, sendLen);
	if(sent < 0) {
		fprintf(stderr, "Error: sendPDU ret -1\n");
		exit(-1);
	} else if(sent == 0) {
		fprintf(stderr, "Server has terminated\n");
		exit(0);
	}

	printf("Amount of data sent is: %d\n", sent);
}

int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	printf("Enter data: ");
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
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

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 3)
	{
		printf("usage: %s host-name port-number \n", argv[0]);
		exit(1);
	}
}
