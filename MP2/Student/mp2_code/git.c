#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "git.h"

void listenForNeighbors(char* logFile);
void* announceToNeighbors(void* unusedParam);
void* forwardToNeighbors(void* unusedParam);
void* broadcastToNeighbors(void* unusedParam);

int globalMyID = 0;

// store the sequence for broadcasting
int broadcastSequence = 0;

// store the broadcasting sequence from node i
int broadcastSequenceFrom[graphSize] = {0};

//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
struct timeval globalLastHeartbeat[graphSize];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
struct sockaddr_in globalNodeAddrs[graphSize];

// record neighbour nodes, 1 --> connected, 0 --> not connected
int neighbour[graphSize] = {0};

// record the cost with neighbours
int cost[graphSize] = {1};

// a 2D array that stores the connection between nodes
int network[graphSize][graphSize] = {0};

// an array records the previous node to the destination by the shortest path
int pred[graphSize];

// set to 1 if we want to forward the announcement
int forwardFlag;
int broadcastFlag;
// stores the message from broadcast
char announcementBuf[512];
// used to avoid duplicated broadcast
int announcementSource;
int announcementFrom;
int announcementBufLength;
extern int dumpReceived;

void init_to_x(int* array, int s, int x){
    // initialize an array of size s to value x
    // return: none, update array
    for(int i = 0; i < s; i++){
        array[i] = x;
    }
}

void read_cost(char* file_name, int* cost){
    // arguments: file_name, pointer to the array saving cost for nodes
    // return: none, update the cost array
    // read the cost values from a file and assign them to cost
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    const char s[2] = " ";
    char *token;
    fp = fopen(file_name, "r");
    if (fp == NULL){
		// if the file is not found update cost to 1 for all nodes
        //init_to_x(cost, 256, 1);
        return;
    }
    while ((read = getline(&line, &len, fp)) != -1) {
        //split the line by a space, the first value is the node index
        // the second value is the cost
        token = strtok(line, s);
        int node = atoi(token);
        token = strtok(NULL, s);
        int node_cost = atoi(token);
        cost[node] = node_cost;
    }
    fclose(fp);
    if (line)
        free(line);
}

int main(int argc, char** argv)
{
	if(argc != 4)
	{
		fprintf(stderr, "Usage: %s mynodeid initialcostsfile logfile\n\n", argv[0]);
		exit(1);
	}


	//initialization: get this process's node ID, record what time it is,
	//and set up our sockaddr_in's for sending to the other nodes.
	globalMyID = atoi(argv[1]);
	int i;
	for(i=0;i<graphSize;i++)
	{
		gettimeofday(&globalLastHeartbeat[i], 0);

		char tempaddr[100];
		sprintf(tempaddr, "10.1.1.%d", i);
		memset(&globalNodeAddrs[i], 0, sizeof(globalNodeAddrs[i]));
		globalNodeAddrs[i].sin_family = AF_INET;
		globalNodeAddrs[i].sin_port = htons(7777);
		inet_pton(AF_INET, tempaddr, &globalNodeAddrs[i].sin_addr);
	}


	//TODO: read and parse initial costs file. default to cost 1 if no entry for a node. file may be empty.
	//int cost[255] = {1};
	init_to_x(cost, graphSize, 1); // initialize all to 1 regardless of the cost file, then update it use the cost file
	read_cost(argv[2], cost);


	//socket() and bind() our socket. We will do all sendto()ing and recvfrom()ing on this one.
	if((globalSocketUDP=socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket");
		exit(1);
	}
	char myAddr[100];
	struct sockaddr_in bindAddr;
	sprintf(myAddr, "10.1.1.%d", globalMyID);
	memset(&bindAddr, 0, sizeof(bindAddr));
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(7777);
	inet_pton(AF_INET, myAddr, &bindAddr.sin_addr);
	if(bind(globalSocketUDP, (struct sockaddr*)&bindAddr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind");
		close(globalSocketUDP);
		exit(1);
	}


	//start threads... feel free to add your own, and to remove the provided ones.
	pthread_t announcerThread;
	pthread_create(&announcerThread, 0, announceToNeighbors, (void*)0);

	pthread_t broadcastThread;
	pthread_create(&broadcastThread, 0, broadcastToNeighbors, (void*)0);


	//pthread_t forwardThread;
	//pthread_create(&forwardThread, 0, forwardToNeighbors, (void*)0);



	//good luck, have fun!
	//pass the log file name to the function
	listenForNeighbors(argv[3]);



}
