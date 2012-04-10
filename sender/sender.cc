#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() and alarm() */
#include <errno.h>      /* for errno and EINTR */
#include <signal.h>     /* for sigaction() */
#include "buffer.h"     /* Holds the buffer variable of size 4096 */

#define ECHOMAX         255     /* Longest string to echo */
#define TIMEOUT_SECS    2       /* Seconds between retransmits */
#define MAXTRIES        5       /* Tries before giving up */

const unsigned int MAXCHUNK = 512;

int tries=0;   /* Count of times sent - GLOBAL for signal-handler access */

/*
	The message format to send to receiver.
*/
struct message
{
	int type;
	int seqno;
	int length;
	char data[512];
};

void DieWithError(char *errorMessage);   /* Error handling function */
void CatchAlarm(int ignored);            /* Handler for SIGALRM */

int main(int argc, char *argv[])
{
    int sock;                        /* Socket descriptor */
    struct sockaddr_in echoServAddr; /* Echo server address */
    struct sockaddr_in fromAddr;     /* Source address of echo */
    unsigned short echoServPort;     /* Echo server port */
    unsigned int fromSize;           /* In-out of address size for recvfrom() */
    struct sigaction myAction;       /* For setting signal handler */
    char *servIP;                    /* IP address of server */
    char *echoString;                /* String to send to echo server */
    char echoBuffer[ECHOMAX+1];      /* Buffer for echo string */
    int echoStringLen;               /* Length of string to echo */
    int respStringLen;               /* Size of received datagram */

	unsigned int servPort, chunkSize, windowSize;
	if(argc < 5 || argc > 5)
	{
		fprintf(stderr, "Usage: %s <Server IP> <Server Port> <Chunk Size> <Window Size>\n", argv[0]);
		exit(1);
	}

    servIP = argv[1];           /* First arg:  server IP address (dotted quad) */
	servPort = atoi(argv[2]);
	chunkSize = atoi(argv[3]);
	windowSize = atoi(argv[4]);
	
	if (chunkSize > MAXCHUNK)
	{
		fprintf(stderr, "Error: Chunk size must be less than %u\n", MAXCHUNK);
		exit(1);
	}
	printf("Inputs gathered correctly\n");
	return 0;
    if ((echoStringLen = strlen(echoString)) > ECHOMAX)
    {
		char mess[19] = "Echo word too long";
		DieWithError(mess);
	}

    if (argc == 4)
        echoServPort = atoi(argv[3]);  /* Use given port, if any */
    else
        echoServPort = 7;  /* 7 is well-known port for echo service */

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
    memset(&echoServAddr, 0, sizeof(echoServAddr));    /* Zero out structure */
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);  /* Server IP address */
    echoServAddr.sin_port = htons(echoServPort);       /* Server port */

    /* Send the string to the server */
    if (sendto(sock, echoString, echoStringLen, 0, (struct sockaddr *)
               &echoServAddr, sizeof(echoServAddr)) != echoStringLen)
    {
		char mess[56] = "sendto() sent a different number of bytes than expected";
		DieWithError(mess);
  	}

    /* Get a response */
    
    fromSize = sizeof(fromAddr);
    alarm(TIMEOUT_SECS);        /* Set the timeout */
    while ((respStringLen = recvfrom(sock, echoBuffer, ECHOMAX, 0,
           (struct sockaddr *) &fromAddr, &fromSize)) < 0)
        if (errno == EINTR)     /* Alarm went off  */
        {
            if (tries < MAXTRIES)      /* incremented by signal handler */
            {
                printf("timed out, %d more tries...\n", MAXTRIES-tries);
                if (sendto(sock, echoString, echoStringLen, 0, (struct sockaddr *)
                            &echoServAddr, sizeof(echoServAddr)) != echoStringLen)
                {
					char mess[17] = "sendto() failed";
					DieWithError(mess);
                }
				alarm(TIMEOUT_SECS);
            } 
            else
            {
				char mess[12] = "No Response";
				DieWithError(mess);
			}
        } 
        else
		{
			char mess[18] = "recvfrom() failed";
            DieWithError(mess);
		}
    /* recvfrom() got something --  cancel the timeout */
    alarm(0);

    /* null-terminate the received data */
    echoBuffer[respStringLen] = '\0';
    printf("Received: %s\n", echoBuffer);    /* Print the received data */
    
    close(sock);
    exit(0);
}

void CatchAlarm(int ignored)     /* Handler for SIGALRM */
{
    tries += 1;
}
