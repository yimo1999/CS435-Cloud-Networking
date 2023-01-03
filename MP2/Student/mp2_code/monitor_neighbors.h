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
// #include "json.hpp"
using namespace std;
// using json = nlohmann::json;

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

extern short int cost[256][256];


short int sequence_number = 0;
short int updt_number = 0;

typedef struct Information{
	char type[4];
	short int dest;
	char lsaContent[512];
	short int from;
	short int seqNum;
}info;

extern int seqNumArray[256];

extern int changeFlag;

extern char* logFileName;

//parameters for calcuating shortestPath
set<int> q;
int visited[256];
int ShortsPathPrev[256];
int ShortsPathDis[256];

int forwardTable[256];
int updateinfo[256];

int flag;

pthread_mutex_t forwardLSALock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t shortestPathLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t forwardTableCal = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t seq = PTHREAD_MUTEX_INITIALIZER;

void printGraph(){
	cout << "--------------------this is cost graph START------------------" << endl;
	for(int i = 0; i < 256; i++){
		for(int j = 0; j < 256; j++){
			if(cost[i][j]){
				cout << i << ", " << j << ": " << cost[i][j] << endl;
			}

		}
	}
		cout << "--------------------this is cost graph END------------------" << endl;
}


void printCost(){
		cout << "--------------------neighbors START------------------" << endl;
	for(int i = 0; i < 256; i++){
		if(neighbors[i]){
			cout << globalMyID << ", " << i << ": " << cost[globalMyID][i] << endl;
		}
	}
			cout << "--------------------neighbors END------------------" << endl;
}

void writeLogfile(const char* text)
{
	FILE* fp;
	fp = fopen(logFileName, "a");
	if(fp == NULL){
		cout << "error" << endl;
		exit(1);
	}
	fwrite(text, 1, strlen(text), fp);
	fclose(fp);
}




void* forwardLSA(void* unusedParam){
	// pthread_mutex_lock(&forwardLSALock);

	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 500 * 1000 * 1000; //300 ms
	while(1){

		sequence_number++;
		seqNumArray[globalMyID] = sequence_number;

			char message[512];
			short int from = htons((short int) globalMyID);
			short int seqTemp = htons(sequence_number);

			strcpy(message, "lsaa");
			memcpy(message + 4 + sizeof(short int), &from, sizeof(short int));
			memcpy(message + 4 + sizeof(short int) * 2, &seqTemp, sizeof(short int));
			int sizeTemp = 4 + sizeof(short int) * 3;
			int j = 0;
			for(short int i = 0; i < 256; i++){
				if(neighbors[i] == 1){
					short int nei = htons((short int)i);
					short int cst = htons(neighbors_cost[i]);
					// cout << nei << endl;
					memcpy(message + sizeTemp + sizeof(short int) * j, &nei, sizeof(short int));
					memcpy(message + sizeTemp + sizeof(short int) * (j + 1), &cst, sizeof(short int));
					j += 2;
				}
			}
			short int len = sizeTemp + j * sizeof(short int);
			short int len_t = htons(len);
			memcpy(message + 4, &len_t, sizeof(short int));

				for(int i=0;i<256;i++){
					if(i != globalMyID){
						sendto(globalSocketUDP, message, len + 1, 0,
							(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
					}
				}
				nanosleep(&sleepFor, 0);
	}




// pthread_mutex_unlock(&forwardLSALock);
}


void hackyBroadcast(const char* buf, int length)
{
	int i;
	for(i=0;i<256;i++)
		if(i != globalMyID) //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0,
				  (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
}



void* announceToNeighbors(void* unusedParam)
{

	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
	while(1)
	{
		// heartBeating();
		// forwardLSA();
		hackyBroadcast("h", 1);
		// forwardLSA();
		nanosleep(&sleepFor, 0);
	}
}




// decode
void decodeLSA(char* buf){
	// memset(updt, 0, sizeof(*updt));
	short int len;
	short int seqTemp;
	short int from;
	short int nei;
	short int cst;
	short int costTemp[256];
	// memcpy(costTemp, cost, sizeof(short int) * 256 * 256);
	memcpy(&len, buf + 4, sizeof(short int));
	memcpy(&from, buf + 4 + sizeof(short int), sizeof(short int));
	memcpy(&seqTemp, buf + 4 + sizeof(short int) * 2, sizeof(short int));
	short int len_t = ntohs(len);
	short int from_t = ntohs(from);
	short int seqTemp_t = ntohs(seqTemp);
	for(int i = 0; i < 256; i++){
		costTemp[i] = 0;
	}
	if(seqTemp_t > seqNumArray[from_t]){
		seqNumArray[from_t] = seqTemp_t;
		int cnt = 3;
		short int nei_t;
		short int cst_t;
		while(4 + sizeof(short int) * cnt < len_t){
			memcpy(&nei, buf + 4 + sizeof(short int) * cnt, sizeof(short int));
			memcpy(&cst, buf + 4 + sizeof(short int) * (cnt + 1), sizeof(short int));
			nei_t = ntohs(nei);
			cst_t = ntohs(cst);
			cnt += 2;
			// cout << nei_t << endl;
			// cout << cst_t << endl;
			// costTemp[from_t][nei_t] = cst_t;
			// costTemp[nei_t][from_t] = cst_t;
			costTemp[nei_t] = cst_t;
			// cout << costTemp[nei_t] << endl;
		}

		// memcpy(cost, costTemp, sizeof(short int) * 256 * 256);
		for(int i = 0; i < 256; i++){
			cost[from_t][i] = costTemp[i];
			cost[i][from_t] = costTemp[i];
			// if(costTemp[i]){
			// 	cout << costTemp[i] << "----" << endl;
			// }

		}

		for(int i=0;i<256;i++){
			if(i != globalMyID && neighbors[i] == 1 && i != from_t){
				sendto(globalSocketUDP, buf, len_t + 1, 0,
					(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
			}
		}
	}
	// cout << len << endl;
	// cout << len_t << endl;



}

void JoinNetInfo(){
	info *joininfo = new info;
	memset(joininfo, 0, sizeof(*joininfo));
	strcpy(joininfo->type, "join");
	joininfo->from = globalMyID;
	for(int i=0;i<256;i++){
		if(i != globalMyID){
			sendto(globalSocketUDP, joininfo, sizeof(*joininfo), 0,	(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
		}
	}

}

void generateGraph(info* graph){

	// sleep(1);
	memset(graph, 0, sizeof(*graph));
	strcpy(graph->type, "updt");
	graph->from = globalMyID;
	string graphinfo = "";
	for(int i = 0; i < 256; i++){
		for(int j = 0; j < 256; j++){
			if(cost[i][j] > 0){
				string node1 = to_string(i);
				string node2 = to_string(j);
				string strCost = to_string(cost[i][j]);
				graphinfo += node1 + "," + node2 + "," + strCost + ",";
			}
		}
	}

	char *st1 = const_cast<char *>(graphinfo.c_str()) ;
	cout << st1 << endl;
	strcpy(graph->lsaContent, st1);
	cout << graph->lsaContent << endl;

}


void decodeGraph(char* buf){
	flag = 1;
	char *token;
	// cout << "recv update from node " << updt->nodeID << endl;
	token = strtok(buf, ",");
	int idx = 0;
	int x;
	int y;
	int c;
	while(token != NULL ) {
		if(idx == 0){
			// cout << "x: " << atoi(token) << endl;
			x = atoi(token);
			idx = 1;
		}else if(idx == 1){
			// cout << "y: " << atoi(token) << endl;
			y = atoi(token);
			idx = 2;
		}else if(idx == 2){
			// cout << "cost: " << atoi(token) << endl;
			c = atoi(token);
			idx = 0;
		}

		token = strtok(NULL, ",");
		if(idx == 0){
			cost[x][y] = c;
			cost[y][x] = c;
		}
	}
}


void forwardingTable(){
	// pthread_mutex_lock(&forwardTableCal);
	memset(forwardTable, 0, sizeof(*forwardTable));
	forwardTable[globalMyID] = globalMyID;
	for(int i = 0; i < 256; i++){
		forwardTable[i] = -1;
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
		forwardTable[i] = next;
	}
}

}


int forwardTableForOne(int dest){

	if(ShortsPathPrev[dest] == -1){
		return -1;
	}
	while(ShortsPathPrev[dest] != globalMyID){
		dest = ShortsPathPrev[dest];
	}
	return dest;
}

void shortestPath(){
	pthread_mutex_lock(&shortestPathLock);


	for(int i = 0; i < 256; i++){
		ShortsPathPrev[i] = -1;
		ShortsPathDis[i] = MAXINT;
		visited[i] = 0;



	}


	ShortsPathDis[globalMyID] = 0;
	ShortsPathPrev[globalMyID] = globalMyID;
	while(1){
		int minEdge = -1;
		int minCost = MAXINT;
		for(int i = 0; i < 256; i++){
			if(ShortsPathDis[i] < minCost && visited[i] == 0){
				minCost = ShortsPathDis[i];
				minEdge = i;
		    }
		}

		visited[minEdge] = 1;
		q.insert(minEdge);
		for(int j = 0; j < 256; j++){
			if(visited[j] != 1 && cost[minEdge][j] + ShortsPathDis[minEdge] < ShortsPathDis[j] && cost[minEdge][j] > 0){
				ShortsPathDis[j] = cost[minEdge][j] + ShortsPathDis[minEdge];
				ShortsPathPrev[j] = minEdge;
			}else if(visited[j] != 1 && cost[minEdge][j] + ShortsPathDis[minEdge] == ShortsPathDis[j] && cost[minEdge][j] > 0){
				int sourceArrayforMinEdge[256] = {0};
				int sourceArrayforJ[256] = {0};
				int tempMin = minEdge;
				int tempJ = ShortsPathPrev[j];
				int cntMin = 0;
				int cntJ = 0;
				while(tempMin != globalMyID){
					sourceArrayforMinEdge[cntMin] = tempMin;
					tempMin = ShortsPathPrev[tempMin];
					cntMin++;
				}
				cntMin--;
				while(tempJ != globalMyID){
					sourceArrayforJ[cntJ] = tempJ;
					tempJ = ShortsPathPrev[tempJ];
					cntJ++;
				}
				cntJ--;

				while(cntMin >= 0 && cntJ >= 0){
					if(sourceArrayforMinEdge[cntMin] == sourceArrayforJ[cntJ]){
						cntMin--;
						cntJ--;
					}else{
						break;
					}
				}

				if(sourceArrayforMinEdge[cntMin] < sourceArrayforJ[cntJ]){
					ShortsPathDis[j] = cost[minEdge][j] + ShortsPathDis[minEdge];
					ShortsPathPrev[j] = minEdge;
				}

			}
		}
		if(minCost == MAXINT){
			break;
		}

		if(q.size() == 256){
			break;
		}
	}

	q.clear();
	// forwardingTable();
	pthread_mutex_unlock(&shortestPathLock);
}

// check time out
void* checkTimeout(void* unusedParam){

		while(1){
			struct timeval currTime;
			gettimeofday(&currTime, 0);
			for(int i = 0; i < 256; i++){



				if(neighbors[i] == 1 && currTime.tv_sec - globalLastHeartbeat[i].tv_sec > 2){

						cout << "node " << i << " timeout" << endl;
						cost[globalMyID][i] = 0;
						cost[i][globalMyID] = 0;
						neighbors[i] = 0;
						// sequence_number++;


				}
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
	FILE* fp;
	fp = fopen(logFileName, "a");
	fclose(fp);
	while(1)
	{
		info* msg = new info;
		memset(msg, 0, sizeof(*msg));
		memset(recvBuf, '\0', 1000);
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 1000, 0,
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
				perror("connectivity listener: recvfrom failed");
				exit(1);
		}

		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);
		short int heardFrom = -1;
		// cout << recvBuf << endl;
		if(strstr(fromAddr, "10.1.1."))
		{
			heardFrom = atoi(
					strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);

			// cout << heardFrom << endl;
			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.
			// cout << "htbt from " << heardFrom << endl;
			if(!neighbors[heardFrom]){
				cout << "1st time recv htbt from node " << heardFrom << endl;

				// forwardLSA();

				neighbors[heardFrom] = 1;
				cost[heardFrom][globalMyID] = neighbors_cost[heardFrom];
				cost[globalMyID][heardFrom] = neighbors_cost[heardFrom];
				// forwardLSA();

				// sequence_number++;
				// seqNumArray[globalMyID] = sequence_number;
				// printGraph();
			}
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
		}
		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
    const char* p = (const char*)recvBuf;
		if(!strncmp(p, "send", 4))
		{
			//TODO send the requested message to the requested destination node
			// ...
			short int destTemp;
			char message[256];

			memcpy(&destTemp, recvBuf + 4, sizeof(short int));
			strcpy(message, p + 4 + sizeof(short int));
			// cout << destTemp << endl;
			destTemp = ntohs(destTemp);
			// cout << destTemp << endl;
			// cout << message << endl;
			shortestPath();
			printCost();
			printGraph();

			if(destTemp == globalMyID){
				// receive packet message [text text text]
				string text = message;
				string temp = "receive packet message " + text + "\n";

				cout << temp << endl;
				const char* content = temp.c_str();
				writeLogfile(content);

			}else{
				int nxt = forwardTableForOne(destTemp);
				// printGraph();

				// cout << "dest " << msg->dest << " next hop is " << forwardTable[msg->dest] << endl;
				if(nxt == -1){
					// unreachable dest [nodeid]
					string nodeid_dest = to_string(destTemp);
					string temp = "unreachable dest " + nodeid_dest + "\n";

					cout << temp << endl;
					const char* content = temp.c_str();
					writeLogfile(content);

				}else{
					if(heardFrom == -1){
						// sending packet dest [nodeid] nexthop [nodeid] message [text text]
						string nodeid_dest = to_string(destTemp);
						string nodeid_nxtHop = to_string(nxt);
						string text = message;
						string temp = "sending packet dest " + nodeid_dest + " nexthop " + nodeid_nxtHop + " message " + text + "\n";

						cout << temp << endl;
						const char* content = temp.c_str();
						writeLogfile(content);

						sendto(globalSocketUDP, recvBuf, 4 + sizeof(short int) + strlen(message), 0,
								(struct sockaddr*)&globalNodeAddrs[nxt], sizeof(globalNodeAddrs[nxt]));

					}else{
						// forward packet dest [nodeid] nexthop [nodeid] message [text text]
						string nodeid_dest = to_string(destTemp);
						string nodeid_nxtHop = to_string(nxt);
						string text = message;
						string temp = "forward packet dest " + nodeid_dest + " nexthop " + nodeid_nxtHop + " message " + text + "\n";

						cout << temp << endl;
						const char* content = temp.c_str();
						writeLogfile(content);
						sendto(globalSocketUDP, recvBuf, 4 + sizeof(short int) + strlen(message), 0,
								(struct sockaddr*)&globalNodeAddrs[nxt], sizeof(globalNodeAddrs[nxt]));
					}

				}

			/*	cout << "msg from node: " << heardFrom << endl;
				cout << msg->type << " to dest " << msg->dest << " " << msg->content << endl;
				cout << "next hop is " << forwardTable[msg->dest] << endl; */

				//send to next hop;
			}
		}
		//'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
		else if(!strncmp(p, "lsaa", 4)){
			pthread_mutex_lock(&forwardLSALock);
			// cout << "recv lsa from node " << msg->from << endl;
			// cout << "seqNum stored is " << seqNumArray[msg->from] << ", what we got is " << msg->seqNum << endl;
			char *buf = (char*) recvBuf;
			decodeLSA(buf);


			pthread_mutex_unlock(&forwardLSALock);
		}


		//TODO now check for the various types of packets you use in your own protocol
		//else if(!strncmp(recvBuf, "your other message types", ))
		// ...
	}
	//(should never reach here)
	close(globalSocketUDP);
}
