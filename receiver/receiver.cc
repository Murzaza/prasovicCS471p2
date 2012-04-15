#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket() and bind() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <signal.h>
#include <errno.h>
#include "structs.h"
#include <vector>
#include <string>
#include <time.h>

#define MAXSIZE 4096     /* Longest string to echo */

using namespace std;
int tries = 0;

void DieWithError(char *errorMessage);  /* External error handling function */
void CatchAlarm(int ignored);

int main(int argc, char *argv[])
{
    int sock;                        /* Socket */
    unsigned int cliAddrLen;         /* Length of incoming message */
    struct message retBuffer;        /* Buffer for echo string */
	vector<message> retMess;
	string finalOut = "";    
	int recvMsgSize;                 /* Size of received message */

	size_t messSize = sizeof(struct message);
	size_t ackSize = sizeof(struct ack);

	unsigned short servPort;
	struct sockaddr_in servAddr, clntAddr;
    struct ack ret;
	int failRate = 0;
	int nextPkt = 0;
	int lastAck = -1;
	struct sigaction myAction;
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
	
	myAction.sa_handler = CatchAlarm;
	if(sigfillset(&myAction.sa_mask) < 0)
	{
		char m[20] = "sigfillset() failed";
		DieWithError(m);
	}
	myAction.sa_flags = 0;

	if(sigaction(SIGALRM, &myAction, 0) < 0)
	{
		char m[31] = "sigaction() failed for SIGALRM";
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

	for (;;) /* Run forever */
    {
        /* Set the size of the in-out parameter */
        cliAddrLen = sizeof(clntAddr);

        /* Block until receive message from a client */
        if ((recvMsgSize = recvfrom(sock, &retBuffer, messSize, 0,
            (struct sockaddr *) &clntAddr, &cliAddrLen)) < 0)
        {
			close(sock);
			char m[18]="recvfrom() failed";
		    DieWithError(m);
		}
		printf("%i <-buffer:next-> %i\n", retBuffer.seqno, nextPkt);
		if(retBuffer.seqno == nextPkt)
		{	
			//Calculate some variables.
			finalOut = finalOut + retBuffer.data;
			lastAck = retBuffer.seqno;
			nextPkt += retBuffer.length;
			printf("trueType %i\n", retBuffer.type);	
			if(retBuffer.type == 4)
			{
				printf("TYPE 8\n");
				ret.type = 8;
				time_t start = time(NULL);
				time_t end = time(NULL);

				ret.ackno = lastAck;
				if(sendto(sock, &ret, ackSize, 0, (struct sockaddr *) &clntAddr, sizeof(clntAddr)) != ackSize)
				{
					char m[56] = "sendto() sent a different number of bytes than expected";
					DieWithError(m);
				}
				//alarm(2);
				/*
				while(end - start < 2)
				{
					while(recvfrom(sock, &retBuffer, messSize, 0, (struct sockaddr *) &clntAddr, &cliAddrLen) < 0);
					{
						if(errno == EINTR)
						{
							break;			
						}
					}	
					if(retBuffer.type == 4)
					{
						if(sendto(sock, &ret, ackSize, 0, (struct sockaddr *) &clntAddr, sizeof(clntAddr)) != ackSize)
						{
							char m[56] = "sendto() sent a different number of bytes than expected";
							DieWithError(m);
						}
					}
					end = time(NULL);
				}
				*/
				printf("LEAVING!\n");
				lastAck = -1;
				nextPkt = 0;
				finalOut = "";
				memset(&ret, 0, sizeof(ack));
			}
			else
			{
				puts("TYPE 2\n");
				ret.type = 2;
				ret.ackno = retBuffer.seqno;

				//Create correct output.
				printf("---- RECEIVING %i\n", retBuffer.seqno);
				printf("SENDING ACK %i\n", ret.ackno);

	        	// Send ack back to client.
				if (sendto(sock, &ret, ackSize, 0, 
    	         (struct sockaddr *) &clntAddr, sizeof(clntAddr)) != ackSize)
	        	{
					close(sock);
					char m[56] = "sendto() sent a different number of bytes than expected";
		    		DieWithError(m);
    			}
			}
		}
		else
		{
			//Resend last ack sent.
			printf("SENDING ACK %i\n", ret.ackno);
			if(sendto(sock, &ret, ackSize, 0, (struct sockaddr *) &clntAddr, sizeof(clntAddr)) != ackSize)
			{	
				close(sock);
				char m[56] = "sento() sent a different number of bytes than expected";
				DieWithError(m);
			}
		}
	}
}

void CatchAlarm(int ignored)
{
	tries += 1;
}
