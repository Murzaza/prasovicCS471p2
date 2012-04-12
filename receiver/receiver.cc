#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket() and bind() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include "structs.h"
#include <vector>
#include <string>

#define MAXSIZE 4096     /* Longest string to echo */

using namespace std;

void DieWithError(char *errorMessage);  /* External error handling function */

int main(int argc, char *argv[])
{
    int sock;                        /* Socket */
    unsigned int cliAddrLen;         /* Length of incoming message */
    struct message retBuffer;        /* Buffer for echo string */
	vector<message> retMess;
	string finalOut;    
	int recvMsgSize;                 /* Size of received message */

	size_t messSize = sizeof(struct message);
	size_t ackSize = sizeof(struct ack);

	unsigned short servPort;
	struct sockaddr_in servAddr, clntAddr;
    struct ack ret;
	int failRate = 0;

	if (argc < 2)         /* Test for correct number of parameters */
    {
        fprintf(stderr,"Usage:  %s <UDP SERVER PORT> [<FAIL RATE>]\n", argv[0]);
        exit(1);
    }

	if (argc == 3)
	{
		failRate = atoi(argv[2]);		
	}
    servPort = atoi(argv[1]);  /* First arg:  local port */

    /* Create socket for sending/receiving datagrams */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
		char m[16] = "socket() failed";
		DieWithError(m);
	}

    /* Construct local address structure */
    memset(&servAddr, 0, sizeof(servAddr));   /* Zero out structure */
    servAddr.sin_family = AF_INET;                /* Internet address family */
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    servAddr.sin_port = htons(servPort);      /* Local port */

    /* Bind to the local address */
    if (bind(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
    {
		char m[14] = "bind() failed";
	    DieWithError(m);
  	}
	int ackC = -1;
    for (;;) /* Run forever */
    {
        /* Set the size of the in-out parameter */
        cliAddrLen = sizeof(clntAddr);

        /* Block until receive message from a client */
        if ((recvMsgSize = recvfrom(sock, &retBuffer, messSize, 0,
            (struct sockaddr *) &clntAddr, &cliAddrLen)) < 0)
        {
			char m[18]="recvfrom() failed";
		    DieWithError(m);
		}

        printf("Handling client %s\n", inet_ntoa(clntAddr.sin_addr));
		ret.type = 2;
		ret.ackno = retBuffer.seqno;
        /* Send received datagram back to the client */
        if (sendto(sock, &ret, ackSize, 0, 
             (struct sockaddr *) &clntAddr, sizeof(clntAddr)) != ackSize)
        {
			char m[56] = "sendto() sent a different number of bytes than expected";
		    DieWithError(m);
    	}
	}
    /* NOT REACHED */
}
