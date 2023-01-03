#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>


#include <sys/time.h>
#include <iostream>
#include <fstream>
#include <ostream>
using namespace std;

#include "monitor_neighbors.h"


void listenForNeighbors();
void* announceToNeighbors(void* unusedParam);


int globalMyID = 0;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
struct sockaddr_in globalNodeAddrs[256];

//set all cost to 0 as default.
int cost[256][256];

int neighbors[256];

int neighbors_cost[256];

int changeFlag = 1;

// typedef struct Message{
// 	char type[4];
// 	short int dest;
// 	char content[1000];
// 	short int from;
// 	short int seqNum;
// 	short int lsaContent[256];
// }message;

message lsaArray[256];

int seqNumArray[256];

char* logFileName;

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
	for(i=0;i<256;i++)
	{
		gettimeofday(&globalLastHeartbeat[i], 0);

		char tempaddr[100];
		sprintf(tempaddr, "10.1.1.%d", i);
		memset(&globalNodeAddrs[i], 0, sizeof(globalNodeAddrs[i]));
		globalNodeAddrs[i].sin_family = AF_INET;
		globalNodeAddrs[i].sin_port = htons(7777);
		inet_pton(AF_INET, tempaddr, &globalNodeAddrs[i].sin_addr);
		// cout << tempaddr << endl;
	}


	//TODO: read and parse initial costs file. default to cost 1 if no entry for a node. file may be empty.



  string s = "";
  char* p;

  // const char* d = " ";
	logFileName = argv[3];
  ifstream in(argv[2], ios::in);
  if(!in){
    cout << "error11" << endl;
    return 0;
  }

  // get all lines from the file
  // the 1st value is the connected node, the 2nd value is the cost.
  while(getline(in, s)){
		int node;
		int cst = 1;
		int idx = 0;
		char* temp = new char[strlen(s.c_str())+1];
		strcpy(temp, s.c_str());

	    p = strtok(temp, " ");

	    while(p){
	        if(idx == 0){
	            node = atoi(p);

	            idx++;
	        }else{
	            cst = atoi(p);

	            idx++;
	        }
					// cout << "idx is " << idx << endl;
	        p = strtok(NULL, " ");
	    }
		neighbors[node] = 1;
		neighbors_cost[node] = cst;
		cost[globalMyID][node] = cst;
		cost[node][globalMyID] = cst;
	}

	changeFlag = 1;
	// for(int i = 0; i < 6; i++){
	// 	for(int j = 0; j < 6; j++){
	// 		cout << i << " " << j << " " << cost[i][j] << endl;
	// 	}
	// }
	in.close();

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

	// forwardLSA();
	//start threads... feel free to add your own, and to remove the provided ones.
	pthread_t announcerThread;
	pthread_create(&announcerThread, 0, announceToNeighbors, (void*)0);

	pthread_t forwardLSAThread;
	pthread_create(&forwardLSAThread, 0, forwardLSA, (void*)0);

	pthread_t checkTimeoutThread;
	pthread_create(&checkTimeoutThread, 0, checkTimeout, (void*)0);


	//good luck, have fun!
	// forwardLSA();
	listenForNeighbors();




}
