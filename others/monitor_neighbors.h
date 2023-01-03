#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#include <iostream>
#include <fstream>
#include <ostream>
#include <sys/time.h>
#include <bits/stdc++.h>
#include <set>
#include <limits.h>
#include "json.hpp"
using namespace std;
using json = nlohmann::json;

#define MAXINT 99999

extern int globalMyID;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
extern struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
extern int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
extern struct sockaddr_in globalNodeAddrs[256];

//store connected nodes. if the node connect to node i, then neighbors[i] = 1
//otherwise, neighbors[i] = 0 as default
extern int neighbors[256];

extern int neighbors_cost[256];

extern int cost[256][256];

short int sequence_number = 0;

typedef struct Message{
	char type[4];
	short int dest;
	char content[1000];
	short int from;
	short int seqNum;
	short int lsaContent[256];
}message;

extern int seqNumArray[256];

extern message lsaArray[256];

extern int changeFlag;

extern char* logFileName;

//parameters for calcuating shortestPath
set<int> q;
int visited[256];
int ShortsPathPrev[256];
int ShortsPathDis[256];

int forwardTable[256];
int forwardTableTemp[256];

typedef struct Update{
	char type[4];
	short int nodeID;
	short int graph[256];
}update;


pthread_mutex_t forwardLSALock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t shortestPathLock = PTHREAD_MUTEX_INITIALIZER;

void printGraph(){
	cout << "--------------------this is cost graph START------------------" << endl;
	for(int i = 0; i < 6; i++){
		for(int j = 0; j < 6; j++){
			cout << i << ", " << j << ": " << cost[i][j] << endl;
		}
	}
		cout << "--------------------this is cost graph END------------------" << endl;
}

void writeLogfile(char* text){
	FILE* fp;
	fp = fopen(logFileName, "a");
	if(fp == NULL){
		cout << "error" << endl;
		exit(1);
	}
	fwrite(text, 1, strlen(text), fp);
	fclose(fp);
}

void heartBeating(){
	message *heartBeat = new message;
	memset((char*)heartBeat, 0, sizeof(*heartBeat));
	strcpy(heartBeat->type, "htbt");
	heartBeat->from = globalMyID;
	for(int i = 0; i < 256; i++){
			heartBeat->lsaContent[i] = cost[globalMyID][i];
	}
	for(int i=0;i<256;i++){
		if(i != globalMyID){
			sendto(globalSocketUDP, heartBeat, sizeof(*heartBeat), 0,	(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
		}
	}
}

void* announceToNeighbors(void* unusedParam)
{
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
	while(1)
	{
		heartBeating();
		nanosleep(&sleepFor, 0);
	}
}

void* forwardLSA(void* unusedParam){
	while(1){
		if(changeFlag == 1){
			sequence_number++;
			message *lsainfo = new message;
			memset((char*)lsainfo, 0, sizeof(*lsainfo));
			strcpy(lsainfo->type, "lsaa");
			lsainfo->from = globalMyID;
			lsainfo->seqNum = sequence_number;
			// cout << "node " << globalMyID <<" generate LSA" << endl;
			// need to edit after
			for(int i = 0; i < 6; i++){
				cout << i << ": " << cost[i][globalMyID] << endl;
				lsainfo->lsaContent[i] = cost[i][globalMyID];
			}
			// cout << "node " << globalMyID <<" generate LSA finish" << endl;

			for(int i=0;i<256;i++){
				if(i != globalMyID){
					sendto(globalSocketUDP, lsainfo, sizeof(*lsainfo), 0,	(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
				}
			}
			changeFlag = 0;
		}else{
			continue;
		}
	}
}

void forwardingTable(){
	memset(forwardTableTemp, 0, sizeof(*forwardTableTemp));
	forwardTableTemp[globalMyID] = globalMyID;
	for(int i = 0; i < 256; i++){
	if(i == globalMyID){
		continue;
	}
	else if(ShortsPathPrev[i] == -1){
		continue;
	}else{
		int j = i;
		int next;
		while(ShortsPathDis[j] != 0){
			next = j;
			j = ShortsPathPrev[j];
		}
		forwardTableTemp[i] = next;
	}
}
	memset(ShortsPathPrev, 0, sizeof(*ShortsPathPrev));
	memset(ShortsPathDis, 0, sizeof(*ShortsPathDis));
	memcpy(forwardTable, forwardTableTemp, (sizeof(int) * sizeof(forwardTableTemp)) / sizeof(forwardTableTemp[0]));
	memset(forwardTableTemp, 0, sizeof(*forwardTableTemp));
	// cout << "------------forwardTable START----------" << endl;
	// for(int i = 0; i < 6; i++){
	// 		cout << "node " << i << ": " <<forwardTable[i] << endl;
	// }
	// 	cout << "------------forwardTable END----------" << endl;
}

void shortestPath(){
	pthread_mutex_lock(&shortestPathLock);
	for(int i = 0; i < 256; i++){
		ShortsPathPrev[i] = -1;
		ShortsPathDis[i] = MAXINT;
	}
	ShortsPathDis[globalMyID] = 0;
	ShortsPathPrev[globalMyID] = globalMyID;
	while(1){
		int minEdge = -1;
		int minCost = MAXINT;
		for(int i = 0; i < 256; i++){
			if(visited[globalMyID] == 1){
				if((ShortsPathDis[i] < minCost && i != globalMyID) && (visited[i] == 0 || visited[ShortsPathPrev[i]] == 0)){
					minCost = ShortsPathDis[i];
					minEdge = i;
		    	}
			}else{
				if(ShortsPathDis[i] < minCost){
					minCost = ShortsPathDis[i];
					minEdge = i;
			}
			}
		}
		for(int j = 0; j < 256; j++){
			if(minEdge != j && cost[minEdge][j] + ShortsPathDis[minEdge] < ShortsPathDis[j] && cost[minEdge][j] > 0){
				ShortsPathDis[j] = cost[minEdge][j] + ShortsPathDis[minEdge];
				ShortsPathPrev[j] = minEdge;
			}
		}
		if(minCost == MAXINT){
			break;
		}
		visited[minEdge] = 1;
		q.insert(minEdge);
		if(q.size() == 256){
			break;
		}
	}
	printGraph();
	// cout << "-------i    Dis      Prev---------" << endl;
	// for(int i = 0; i < 9; i++){
	// 	cout << i << ", " << ShortsPathDis[i] << ", " << ShortsPathPrev[i] << endl;
	// }
	// 	cout << "=============================" << endl;
	q.clear();
	memset(visited, 0, sizeof(*visited));
	forwardingTable();
	pthread_mutex_unlock(&shortestPathLock);
}


void* checkTimeout(void* unusedParam){

	while(1){
		int ifTimeout = 0;
		struct timeval currTime;

		for(int i = 0; i < 256; i++){
			gettimeofday(&currTime, 0);
			if(!neighbors[i]){
				continue;
			}

			if(currTime.tv_sec - globalLastHeartbeat[i].tv_sec > 4){
					ifTimeout = 1;
					cout << "node " << i << " timeout" << endl;
					cost[globalMyID][i] = 0;
					cost[i][globalMyID] = 0;
					neighbors[i] = 0;
					// sequence_number++;
					// message *lsainfo = new message;
					// memset((char*)lsainfo, 0, sizeof(*lsainfo));
					// strcpy(lsainfo->type, "lsaa");
					// lsainfo->from = globalMyID;
					// lsainfo->seqNum = sequence_number;
					// for(int k = 0; k < 256; k++){
					// 	lsainfo->lsaContent[k] = cost[globalMyID][k];globalLastHeartbeat
					// }
					// for(int j=0;j<256;j++){
					// 	if(j != globalMyID && j != i){
					// 		sendto(globalSocketUDP, lsainfo, sizeof(*lsainfo), 0, (struct sockaddr*)&globalNodeAddrs[j], sizeof(globalNodeAddrs[j]));
					// 	}
					// }
					changeFlag = 1;

			}
		}

		if(ifTimeout == 1){
			shortestPath();
			ifTimeout = 0;
		}
	}
}

void listenForNeighbors()
{
	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf[1000];
	int bytesRecvd;
	while(1)
	{
		message* msg = new message;
		memset(msg, 0, sizeof(*msg));
		if ((bytesRecvd = recvfrom(globalSocketUDP, msg, sizeof(*msg), 0,
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
				// perror("connectivity listener: recvfrom failed");
				// exit(1);
		}
		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);
		short int heardFrom = -1;
		if(strstr(fromAddr, "10.1.1."))
		{
			heardFrom = atoi(
					strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);

			// cout << heardFrom << endl;
			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.

			//record that we heard from heardFrom just now.
			// gettimeofday(&globalLastHeartbeat[heardFrom], 0);
		}
		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
    const char* p = (const char*)(char*)recvBuf;
		if(!strncmp(msg->type, "send", 4))
		{
			//TODO send the requested message to the requested destination node
			// ...
			short int destTemp;
			destTemp = ntohs(msg->dest);
			msg->dest = destTemp;
			if(msg->dest == globalMyID){
				cout << msg->content << endl;
			}else{
				// printGraph();
				cout << msg->type << " to dest " << msg->dest << " " << msg->content << endl;
				cout << "next hop is " << forwardTable[msg->dest] << endl;
				msg->dest = htons(msg->dest);
				sendto(globalSocketUDP, msg, sizeof(*msg), 0,
						(struct sockaddr*)&globalNodeAddrs[forwardTable[destTemp]], sizeof(globalNodeAddrs[forwardTable[destTemp]]));
				//send to next hop;
			}
		}
		//'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
		else if(!strncmp(msg->type, "cost", 4))
		{
			//TODO record the cost change (remember, the link might currently be down! in that case,
			//this is the new cost you should treat it as having once it comes back up.)
			// ...
		}
		else if(!strncmp(msg->type, "lsaa", 4)){
			pthread_mutex_lock(&forwardLSALock);
			short int fromTemp;
			short int seqNumTemp;


			if(seqNumArray[msg->from] < msg->seqNum){
				cout << "@@@@@@@@@@@@@@@@@@@@@loop in lsaa@@@@@@@@@@@@@@@@@@" << endl;
				// cout << "lsa source node is " << msg->from << " and heard from node " << heardFrom << endl;
				// cout << "seqNum < what we got seqNum" << endl;
				seqNumArray[msg->from] = msg->seqNum;
				for(int i = 0; i < 256; i++){
					// cout << i << ": "<< msg->lsaContent[i] << endl;
					cost[msg->from][i] = msg->lsaContent[i];
					cost[i][msg->from] = msg->lsaContent[i];
				}
				for(int i=0;i<256;i++){
					if(i != globalMyID && i != msg->from && i != heardFrom){
						sendto(globalSocketUDP, msg, sizeof(*msg), 0,
								(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
					}
				}
				shortestPath();
			}
			pthread_mutex_unlock(&forwardLSALock);
		}else if(!strncmp(msg->type, "htbt", 4)){
			if(neighbors[msg->from] == 0){
				cout << "heartBeat from: " << msg->from << endl;
				cout << "!!!!!!!!!!!!!!!!!!!!!!!!loop in htbt!!!!!!!!!!!!!!!!!!!!!!!" << endl;
				neighbors[msg->from] = 1;
				// cout << "node " << msg->from << " is online" << endl;
				for(int i = 0; i < 256; i++){
					cost[msg->from][i] = msg->lsaContent[i];
					cost[i][msg->from] = msg->lsaContent[i];
				}
				if(cost[msg->from][globalMyID] == 0){
					cost[msg->from][globalMyID] = 1;
					cost[globalMyID][msg->from] = 1;
				}
				shortestPath();
				changeFlag = 1;
				for(int i = 0; i < 256; i++){
					update *updt = new update;
					memset((char*)updt, 0, sizeof(*updt));
					strcpy(updt->type, "updt");
					updt->nodeID = i;
					for(int j = 0; j < 256; j++){
						updt->graph[j] =  cost[i][j];
					}
					sendto(globalSocketUDP, updt, sizeof(*updt), 0,
							(struct sockaddr*)&globalNodeAddrs[msg->from], sizeof(globalNodeAddrs[msg->from]));
				}
			}
			gettimeofday(&globalLastHeartbeat[msg->from], 0);
		}else if(!strncmp(msg->type, "updt", 4)){
			update *u = (update *) msg;
			for(int i = 0; i < 256; i++){
				cost[i][u->nodeID] = u->graph[i];
				cost[u->nodeID][i] = u->graph[i];
			}
			changeFlag = 1;
			shortestPath();
		}
		//TODO now check for the various types of packets you use in your own protocol
		//else if(!strncmp(recvBuf, "your other message types", ))
		// ...
	}
	//(should never reach here)
	close(globalSocketUDP);
}
