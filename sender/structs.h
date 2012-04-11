//Structures needed to communicate back and forth

#ifndef STRUCTS
#define STRUCTS

const unsigned int MAXCHUNK = 512;
 
struct ack
{
	int type;
	int ackno;
};

struct message
{
	int type;
	int seqno;
	int length;
	char data[MAXCHUNK];
};

#endif
