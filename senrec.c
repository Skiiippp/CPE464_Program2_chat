#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "senrec.h"
#include "common.h"

/**
 * Expects flag at start of packet
*/
int sendPDU(int clientSocket, uint8_t *dataBuffer, int lengthOfData) {
    uint16_t pduLength = lengthOfData + PACKET_LEN_SIZE;
    uint8_t pduBuff[MAX_PACKET_SIZE];
    ssize_t bytesSent = 0;
    
    *((uint16_t *)pduBuff) = htons(pduLength);
    memcpy(pduBuff + PACKET_LEN_SIZE, dataBuffer, lengthOfData);

    bytesSent = send(clientSocket, pduBuff, pduLength, 0);

    if(bytesSent < 0) {
        perror("send call");
        return -1;
    }

    return bytesSent;
}

int recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize) {
    uint8_t pduBuff[MAX_PACKET_SIZE];
    uint16_t incomingBuffSize = 0;
    ssize_t bytesRecieved = 0;

    // Recieve header
    if((bytesRecieved = recv(socketNumber, pduBuff, PACKET_LEN_SIZE, MSG_WAITALL)) < 0) {
        if(errno == ECONNRESET) {
            return 0;
        }
        perror("recv call #1");
        return -1;
    } else if(bytesRecieved == 0) {
        return 0;
    }

    incomingBuffSize = ntohs(*(uint16_t *)pduBuff) - PACKET_LEN_SIZE;
    if(incomingBuffSize > bufferSize) {
        fprintf(stderr, "Error: Incoming buff bigger than supplied buff\n");
        return -1;
    }

    if((bytesRecieved = recv(socketNumber, pduBuff + PACKET_LEN_SIZE, incomingBuffSize, MSG_WAITALL)) < 0) {
        if(errno == ECONNRESET) {
            return 0;
        }
        perror("recv call #2");
        return -1;
    }

    memcpy(dataBuffer, pduBuff + PACKET_LEN_SIZE, incomingBuffSize);

    return bytesRecieved;
}

