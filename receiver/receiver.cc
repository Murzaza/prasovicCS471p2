/*
	Mirza Prasovic
	CS471 Spring 2012
	Programming Assignment 2

	Receiver/Server side of the UDP assignment.
*/

#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket() and bind() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <signal.h>
#include <errno.h>
#include "structs.h"
#include <string>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <netdb.h>

#define MAXSIZE 4096     /* Longest string to echo */
#define TIMEOUT_SECS 7

using namespace std;
int tries = 0;

int sock;
void DieWithError(char *errorMessage);  /* External error handling function */
void CatchAlarm(int ignored);   //For when we get a type 4 message
bool isLost(int lossRate);      //If we lose a packet or not

int main(int argc, char *argv[])
{
    unsigned int cliAddrLen;         /* Length of incoming message */
    struct message retBuffer;        /* Buffer for echo string */
	string finalOut = "";			 //Final output message.    
	int recvMsgSize;                 /* Size of received message */

	size_t messSize = sizeof(struct message); //Size of the message struct
	size_t ackSize = sizeof(struct ack);      //Size of the ack struct

	unsigned short servPort;				  // Server port
	struct sockaddr_in servAddr, clntAddr;    // server & client address
    struct ack ret;							  // What to return to the sender.
	int failRate = 0;						  // How often we drop packets
	int nextPkt = 0;						  // Next packet to expect
	int lastAck = -1;						  // Last packet sent.
	struct sigaction myAction;				  // For handling SIGIO event.
	
	//Seeding for random numbers.
	srand(2839);

	//Checking for at least one parameter.
	if (argc < 2)        
    {
        fprintf(stderr,"Usage:  %s <UDP SERVER PORT> [<FAIL RATE>]\n", argv[0]);
        exit(1);
    }

	//Check if the user added a fail rate.
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
	
	//Create action for the Asynchronus I/O
	myAction.sa_handler = CatchAlarm;
	if(sigfillset(&myAction.sa_mask) < 0)
	{
		char m[20] = "sigfillset() failed";
		DieWithError(m);
	}
	myAction.sa_flags = 0;

	if(sigaction(SIGIO, &myAction, 0) < 0)
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
        // Set the size of the in-out parameter 
        cliAddrLen = sizeof(clntAddr);

        // Check if we "lost" a packet.
        while(isLost(failRate))
			// Block until we receive a message from a client. LOST
			if ((recvMsgSize = recvfrom(sock, &retBuffer, messSize, 0,
            	(struct sockaddr *) &clntAddr, &cliAddrLen)) < 0)
        	{
				close(sock);
				char m[18]="recvfrom() failed";
		    	DieWithError(m);
			}
		
		//Actually receive a message here.
		if ((recvMsgSize = recvfrom(sock, &retBuffer, messSize, 0,
           	(struct sockaddr *) &clntAddr, &cliAddrLen)) < 0)
        {
			close(sock);
			char m[18]="recvfrom() failed";
		   	DieWithError(m);
		}
	
		//Check if the received packet is the one we were waiting on.
		if(retBuffer.seqno == nextPkt)
		{	
			//Calculate some variables.
			finalOut = finalOut + retBuffer.data;
			lastAck = retBuffer.seqno;
			//nextPkt += retBuffer.length;
			nextPkt += 1;
			//If the packet is a teardown message, type = 4
			if(retBuffer.type == 4)
			{
				//Set the return type as 8
				ret.type = 8;
				
				ret.ackno = lastAck;

				//Print out appropriate messages.
				printf("---- RECEIVING TEARDOWN\n");
				printf("SENDING ACK TEARDOWN\n");
				//Send teardown ack
				if(sendto(sock, &ret, ackSize, 0, (struct sockaddr *) &clntAddr, sizeof(clntAddr)) != ackSize)
				{
					char m[56] = "sendto() sent a different number of bytes than expected";
					DieWithError(m);
				}
				//Print out the final message sent.
				printf("%s\n", finalOut.c_str());

				//setup the handler to work as nonblock and asynchronous.
				if(fcntl(sock, F_SETOWN, getpid()) < 0)
				{
					char m[38] = "Unable to set process owner to socket";
					DieWithError(m);
				}

				if(fcntl(sock, F_SETFL, O_NONBLOCK | FASYNC) < 0)
				{
					char m[44] = "Unable to put socket in nonblock/async mode";
					DieWithError(m);
				}

				//Wait for seven seconds before ending the program.
				sleep(7);
				return 0; //End
			}
			else //Not a teardown packet, therefore a data containing packet.
			{
				ret.type = 2;
				ret.ackno = retBuffer.seqno;

				//Create correct output.
				printf("---- RECEIVING %i\n", retBuffer.seqno);
				printf("SENDING ACK %i\n", ret.ackno);

	        	// Send ack back to client.
				if (sendto(sock, &ret, ackSize, 0, 
    	         (struct sockaddr *) &clntAddr, sizeof(clntAddr)) != ackSize)
	        	{
					char m[56] = "sendto() sent a different number of bytes than expected";
		    		DieWithError(m);
    			}
			}
		}
		else //Packet was not the expect seqno. Repeat last ackno sent.
		{
			//Resend last ack sent.
			printf("SENDING ACK %i\n", ret.ackno);
			if(sendto(sock, &ret, ackSize, 0, (struct sockaddr *) &clntAddr, sizeof(clntAddr)) != ackSize)
			{	
				char m[56] = "sento() sent a different number of bytes than expected";
				DieWithError(m);
			}
		}
	}
}

//For checking if we lost a packet.
bool isLost(int lossRate)
{
	int rv;
	rv = rand() % 100;
	if(rv < lossRate)
		return true;
	else
		return false;
}


//Used for asynchronous I/O
void CatchAlarm(int ignored)
{
	//Declare some variables that we will need.
	size_t rcvd;
	struct sockaddr_storage clntAddr;
	socklen_t clntLen = (socklen_t) sizeof(clntAddr);
	message get;
	ack ret;
	ret.type = 8;
	ret.ackno = -1;
	size_t ackLen = sizeof(ret);
	size_t mesLen = sizeof(get);

	//Get the packet waiting to be received/
	while((rcvd = recvfrom(sock, &get, mesLen, 0, (struct sockaddr *) &clntAddr, &clntLen)) < 0)
	{
		//Check to make sure we haven't encountered an error.
		if(errno != EWOULDBLOCK)
		{
			char m[16] = "recvfrom failed";
			DieWithError(m);
		}
		else 
		{
			//Only reply to teardown messages.
			if(ret.type == 4)
			{
				printf("SENDING ACK TEARDOWN\n");
				if(sendto(sock, &ret, ackLen, 0, (struct sockaddr *) &clntAddr, sizeof(clntAddr)) != ackLen)
				{
					char m[56] = "sendto() sent a different number of bytes than expected";
				}
			}

		}
	}
}

//End
