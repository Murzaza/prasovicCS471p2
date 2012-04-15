/*
	Mirza Prasovic
	CS 471 Spring 2012
	Program 2

	Sender programmer of the UDP protocol.
*/

#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() and alarm() */
#include <errno.h>      /* for errno and EINTR */
#include <signal.h>     /* for sigaction() */
#include <map>
#include <vector>
#include "buffer.h"     /* Holds the buffer variable of size 4096 */
#include "structs.h"

#define ECHOMAX         255     /* Longest string to echo */
#define TIMEOUT_SECS    2       /* Seconds between retransmits */
#define MAXTRIES        10       /* Tries before giving up */

int tries=0;   /* Count of times sent - GLOBAL for signal-handler access */

void DieWithError(char *errorMessage);   /* Error handling function */
void CatchAlarm(int ignored);            /* Handler for SIGALRM */

int main(int argc, char *argv[])
{
    int sock;                        				// Socket descriptor
    struct sockaddr_in servAddr; 					// Echo server address 
    struct sockaddr_in fromAddr;     				// Source address of echo 
    unsigned int fromSize;           				// In-out of address size for recvfrom() 
    struct sigaction myAction;       				// For setting signal handler 
    char *servIP;                    				// IP address of server 
    int ackLen;               						// Size of received datagram 
	unsigned int servPort, chunkSize, windowSize; 	// Cmd line arguments.
	size_t length = sizeof(struct message);
	size_t ackSize = sizeof(struct ack);
	struct message *frags;
	struct ack ret;
	std::map<int, bool> acks;
	std::vector<int> nextAck;

	//Command line argument checking.
	if(argc < 5 || argc > 5)
	{
		fprintf(stderr, "Usage: %s <Server IP> <Server Port> <Chunk Size> <Window Size>\n", argv[0]);
		exit(1);
	}

	//Assign the command line arguments to variables.
    servIP = argv[1];           
	servPort = atoi(argv[2]);
	chunkSize = atoi(argv[3]);
	windowSize = atoi(argv[4]);

	//Check to make sure the chunk size is under 512.
	if (chunkSize > MAXCHUNK)
	{
		fprintf(stderr, "Error: Chunk size must be less than %u\n", MAXCHUNK);
		exit(1);
	}
	printf("Inputs gathered correctly\n");

    /* Create a best-effort datagram socket using UDP */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
		char mess[17] = "socket() failed";
		DieWithError(mess);
	}

    /* Set signal handler for alarm signal */
    myAction.sa_handler = CatchAlarm;
    if (sigfillset(&myAction.sa_mask) < 0) /* block everything in handler */
    {
		char mess[20] = "sigfillset() failed";
		DieWithError(mess);
	}
    myAction.sa_flags = 0;

    if (sigaction(SIGALRM, &myAction, 0) < 0)
	{
		char mess[31] = "sigaction() failed for SIGALRM";
        DieWithError(mess);
	}

    /* Construct the server address structure */
    memset(&servAddr, 0, sizeof(servAddr));    /* Zero out structure */
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(servIP);  /* Server IP address */
    servAddr.sin_port = htons(servPort);       /* Server port */

	/* Parsing out the message into packets */
	int numPacks = BSIZE / chunkSize;
	if(BSIZE % chunkSize != 0)
		numPacks++;

	frags = new struct message[numPacks];
	for(int i = 0; i < numPacks; ++i)
	{
		frags[i].type = 1;
		//Getting message and length.
		if(i != numPacks - 1)
		{
			strncpy(frags[i].data, buffer + (i*chunkSize), chunkSize);
			frags[i].length = chunkSize;
		}
		else
		{
			strcpy(frags[i].data, buffer + (i*chunkSize));
			frags[i].length = strlen(frags[i].data);
		}
		
		//Seqno calculation.
		if(i == 0)
			frags[i].seqno = 0;
		else
			frags[i].seqno = frags[i-1].seqno + frags[i-1].length;
		acks[frags[i].seqno] = false;
		nextAck.push_back(frags[i].seqno);
	}

	//Loop through every packet and send them.
	int sentOut = 0;
	int receivedAcks = 0;
	int withStanding = 0;
	int base = 0; //Index to next ack accepted.

	while(receivedAcks < numPacks)
	{
		//Send out fragments until the whole window is filled.
		while(withStanding < windowSize && sentOut < numPacks)
		{
			printf("SENDING %i\n", frags[sentOut].seqno);
			if(sendto(sock, frags+sentOut, length, 0, (struct sockaddr *) &servAddr, sizeof(servAddr)) != length)
			{
				char mess[56] = "sendto() sent a different number of bytes than expected";
				DieWithError(mess);
			}
			sentOut++;
			withStanding++;
		}
		alarm(TIMEOUT_SECS);

		//Check for a ack reply.
		while((ackLen = recvfrom(sock, &ret, ackSize, 0, (struct sockaddr *) &fromAddr, &fromSize)) < 0)
		{
			//Alarm went off.
			if(errno == EINTR)
			{
				if(tries < MAXTRIES)
				{
					//Send all the withstanding fragments out again.
					for(int i = base; i < base+windowSize; ++i)
					{
						printf("SENDING %i\n", frags[i].seqno);
						if(sendto(sock, frags+i, length, 0, (struct sockaddr *) &servAddr, sizeof(servAddr)) != length)
						{
							char mess[56] = "sendto() sent a different number of bytes than expected";
							DieWithError(mess);
						}
					}
					alarm(TIMEOUT_SECS);
				}
				else
				{
					char m[12] = "No Response";
					DieWithError(m);
				}
			}
		}
		alarm(0);
		//If the ack we received was the next one to get.
		if(nextAck[base] == ret.ackno)
		{
			receivedAcks++;
			withStanding--;
			base++;
			tries = 0;
			acks[ret.ackno] = true;
			printf("---- RECEIVING ACK ackno %i\n", ret.ackno);
		}
	}

	//Send up to 10 teardown messages to complete transfer.
	message term = {4, frags[numPacks-1].seqno+frags[numPacks-1].length, 0, ""};
	printf("SENDING TEARDOWN!\n");
	if(sendto(sock, &term, length, 0, (struct sockaddr *) &servAddr, sizeof(servAddr)) != length)
	{
		char mess[56] = "sendto() sent a different number of bytes than expected";
		DieWithError(mess);
	}	
	tries++;
	alarm(TIMEOUT_SECS);

	//Keep checking until we finally get a reply otherwise we end it.
	while((ackLen = recvfrom(sock, &ret, ackSize, 0, (struct sockaddr *) &fromAddr, &fromSize)) < 0)
	{
		//The alarm went off.
		if(errno == EINTR)
		{
			if(tries < MAXTRIES)
			{
				//Resend the message.
				printf("SENDING TEARDOWN!\n");
				if(sendto(sock, &term, length, 0, (struct sockaddr *) &servAddr, sizeof(servAddr)) != length)
				{
					char mess[56] = "sendto() sent a different number of bytes than expected";
					DieWithError(mess);
				}
				alarm(TIMEOUT_SECS);
			}
			else
			{
				//Fail if the ack message wasn't received.
				char m[25] = "Teardown message failed";
				DieWithError(m);
			}
		}
	}

	//If we made it here. That means everything was sent successfully.
	if(ret.type == 8)
		printf("---- RECEIVING ACK TEARDOWN!\n");
	else
		printf("---- NOT RECEIVING ACK TEARDOWN!\n");
	
	//Ending now!!
	delete [] frags;
	close(sock);
	exit(0);
}

void CatchAlarm(int ignored)     /* Handler for SIGALRM */
{
    tries += 1;
}
