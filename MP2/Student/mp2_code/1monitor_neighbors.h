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
short int init_number = 0;
short int updt_number = 0;

typedef struct Message{
	char type[4];
	short int dest;
	char content[500];
	short int from;
	short int seqNum;
	short int lsaContent[256];
}message;

extern int seqNumArray[256];

int updtNumArray[256];

int initNumArray[256];

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


int updateinfo[256];

typedef struct Update{
	char type[4];
	short int updtNum;
	short int nodeID;
	char lsaContent[1024];
}update;


int flag;
// int flag

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

void writeLogfile(const char* text){
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

	update *updt = new update;
	memset(updt, 0, sizeof(*updt));
	strcpy(updt->type, "htbt");
	updt->nodeID = globalMyID;
	string graphinfo = "";
	for(int i = 0; i < 256; i++){
			if(i != globalMyID && cost[i][globalMyID] > 0){
				string node1 = to_string(i);
				string node2 = to_string(globalMyID);
				string strCost = to_string(cost[i][globalMyID]);
				graphinfo += node1 + "," + node2 + "," + strCost + ",";
			}

	}
	char *st1 = const_cast<char *>(graphinfo.c_str()) ;
	// cout << st1 << endl;
	strcpy(updt->lsaContent, st1);
	// cout << updt->lsaContent << endl;
	for(int i=0;i<256;i++){
		if(i != globalMyID){
			sendto(globalSocketUDP, updt, sizeof(*updt), 0,	(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
		}
	}






	//
	// message *heartBeat = new message;
	// memset((char*)heartBeat, 0, sizeof(*heartBeat));
	// strcpy(heartBeat->type, "htbt");
	// heartBeat->from = globalMyID;
	// // for(int i = 0; i < 256; i++){
	// // 		heartBeat->lsaContent[i] = cost[globalMyID][i];
	// // }
	// for(int i=0;i<256;i++){
	// 	if(i != globalMyID){
	// 		sendto(globalSocketUDP, heartBeat, sizeof(*heartBeat), 0,	(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
	// 	}
	// }
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




// typedef struct Message{
// 	char type[4];
// 	short int dest;
// 	char content[500];
// 	short int from;
// 	short int seqNum;
// 	short int initNum;
// 	short int updtNum;
// 	short int lsaContent[256];
// }message;

void forwardLSA(){
	pthread_mutex_lock(&seq);
	sequence_number++;
	pthread_mutex_unlock(&seq);
	seqNumArray[globalMyID] = sequence_number;
	message *lsainfo = new message;
	memset((char*)lsainfo, 0, sizeof(*lsainfo));
	strcpy(lsainfo->type, "lsaa");
	lsainfo->from = globalMyID;
	lsainfo->seqNum = sequence_number;
	cout << "+++++++++++++++++++++++forwarding LSA++++++++++++++++" << endl;
	for(int i = 0; i < 256; i++){

		lsainfo->lsaContent[i] = cost[i][globalMyID];
		// cout << i << ": " << lsainfo->from << ": " << lsainfo->lsaContent[i] << endl;
	}

		cout << "+++++++++++++++++++++++forwarding LSA++++++++++++++++" << endl;
	for(int i=0;i<256;i++){
		if(i != globalMyID){
			sendto(globalSocketUDP, lsainfo, sizeof(*lsainfo), 0,	(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
		}
	}
}


void JoinNetInfo(){
	message *lsainfo = new message;
	memset(lsainfo, 0, sizeof(*lsainfo));
	strcpy(lsainfo->type, "join");
	lsainfo->from = globalMyID;
	for(int i=0;i<256;i++){
		if(i != globalMyID){
			sendto(globalSocketUDP, lsainfo, sizeof(*lsainfo), 0,	(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
		}
	}
	// sleep(1);
	// cout << "JoinNetInfo LSA here" << endl;
	// forwardLSA();
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



/*	cout << "/////////////////////////i    Dis      Prev///////////////////" << endl;
	for(int i = 0; i < 9; i++){

		cout << i << ", " << ShortsPathDis[i] << ", " << ShortsPathPrev[i] << endl;

	}
	cout << "//////////////////////////////////////////////////////////////" << endl; */
	// memset(ShortsPathPrev, 0, sizeof(*ShortsPathPrev));
	// memset(ShortsPathDis, 0, sizeof(*ShortsPathDis));
	// memcpy(forwardTable, forwardTableTemp, (sizeof(int) * sizeof(forwardTableTemp)) / sizeof(forwardTableTemp[0]));
	// memset(forwardTableTemp, 0, sizeof(*forwardTableTemp));



	// cout << "------------forwardTable START----------" << endl;
	// for(int i = 0; i < 6; i++){
	// 		cout << "node " << i << ": " <<forwardTable[i] << endl;
	// }
	// 	cout << "------------forwardTable END----------" << endl;
	// pthread_mutex_unlock(&forwardTableCal);
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
			}else if(visited[j] != 1 && cost[minEdge][j] + ShortsPathDis[minEdge]== ShortsPathDis[j] && cost[minEdge][j] > 0){
				int backTrackA[40];
				int backTrackB[40];
				int tempA = minEdge;
				int tempB = ShortsPathPrev[j];
				int idxA = 0;
				int idxB = 0;
				while(1){
					if(ShortsPathPrev[tempA] != globalMyID){
						backTrackA[idxA] = tempA;
						idxA++;
						tempA = ShortsPathPrev[tempA];
					}else{
						idxA--;
						break;
					}
				}

				while(1){
					if(ShortsPathPrev[tempB] != globalMyID){
						backTrackB[idxB] = tempB;
						idxB++;
						tempB = ShortsPathPrev[tempB];
					}else{
						idxB--;
						break;
					}
				}



				while(backTrackA[idxA] == backTrackB[idxB]){
					idxA--;
					idxB--;
				}

				if(backTrackA[idxA] < backTrackB[idxB]){
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
	forwardingTable();
	pthread_mutex_unlock(&shortestPathLock);
}


void* checkTimeout(void* unusedParam){

		while(1){
			int ifTimeout = 0;
			struct timeval currTime;

			for(int i = 0; i < 256; i++){
				gettimeofday(&currTime, 0);


				if(neighbors[i] == 1 && currTime.tv_sec - globalLastHeartbeat[i].tv_sec > 2){
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
						// forwardLSA();

				}
			}
			if(ifTimeout == 1){
				forwardLSA();
				printGraph();
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
		// checkTimeout();
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
			if(neighbors[heardFrom] == 0){
				cout << "1st time recv htbt from node " << heardFrom << endl;
				forwardLSA();
				// cout << "heartBeat from: " << msg->from << endl;
				// cout << "!!!!!!!!!!!!!!!!!!!!!!!!loop in htbt!!!!!!!!!!!!!!!!!!!!!!!" << endl;
				neighbors[heardFrom] = 1;
				// cout << "node " << msg->from << " is online" << endl;
				// for(int i = 0; i < 256; i++){
				// 	cost[msg->from][i] = msg->lsaContent[i];
				// 	cost[i][msg->from] = msg->lsaContent[i];
				// }

				cost[heardFrom][globalMyID] = neighbors_cost[heardFrom];
				cost[globalMyID][heardFrom] = neighbors_cost[heardFrom];

				if(cost[heardFrom][globalMyID] <= 0){
					cost[heardFrom][globalMyID] = 1;
					cost[globalMyID][heardFrom] = 1;
				}
				printGraph();
			}
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
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
			shortestPath();
			printGraph();
			if(msg->dest == globalMyID){
				// receive packet message [text text text]
				string text = msg->content;
				string temp = "receive packet message " + text + "\n";

				cout << temp << endl;
				const char* content = temp.c_str();
				writeLogfile(content);

			}else{
				// printGraph();

				// cout << "dest " << msg->dest << " next hop is " << forwardTable[msg->dest] << endl;
				if(forwardTable[msg->dest] == -1){
					// unreachable dest [nodeid]
					string nodeid_dest = to_string(msg->dest);
					string temp = "unreachable dest " + nodeid_dest + "\n";

					cout << temp << endl;
					const char* content = temp.c_str();
					writeLogfile(content);

				}else{
					if(heardFrom == -1){
						// sending packet dest [nodeid] nexthop [nodeid] message [text text]
						string nodeid_dest = to_string(msg->dest);
						string nodeid_nxtHop = to_string(forwardTable[msg->dest]);
						string text = msg->content;
						string temp = "sending packet dest " + nodeid_dest + " nexthop " + nodeid_nxtHop + " message " + text + "\n";

						cout << temp << endl;
						const char* content = temp.c_str();
						writeLogfile(content);

					}else{
						// forward packet dest [nodeid] nexthop [nodeid] message [text text]
						string nodeid_dest = to_string(msg->dest);
						string nodeid_nxtHop = to_string(forwardTable[msg->dest]);
						string text = msg->content;
						string temp = "forward packet dest " + nodeid_dest + " nexthop " + nodeid_nxtHop + " message " + text + "\n";

						cout << temp << endl;
						const char* content = temp.c_str();
						writeLogfile(content);
					}

				}

			/*	cout << "msg from node: " << heardFrom << endl;
				cout << msg->type << " to dest " << msg->dest << " " << msg->content << endl;
				cout << "next hop is " << forwardTable[msg->dest] << endl; */
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
			cout << "recv lsa from node " << msg->from << endl;
			cout << "seqNum stored is " << seqNumArray[msg->from] << ", what we got is " << msg->seqNum << endl;

				if(seqNumArray[msg->from] < msg->seqNum){
					cout << "I pass the 2nd if" << endl;
					// cout << "@@@@@@@@@@@@@@@@@@@@@loop in lsaa@@@@@@@@@@@@@@@@@@" << endl;
					// cout << "lsa source node is " << msg->from << " and heard from node " << heardFrom << endl;
					// cout << "seqNum < what we got seqNum" << endl;
					seqNumArray[msg->from] = msg->seqNum;
					cout << "^^^^^^^^^^^^^^^^" << endl;
					for(int i = 0; i < 256; i++){
							if(msg->lsaContent[i]){
								cout << i << ": " << msg->from << ": " << msg->lsaContent[i] << endl;
								cost[msg->from][i] = msg->lsaContent[i];
								cost[i][msg->from] = msg->lsaContent[i];
							}
					}
					cout << "^^^^^^^^^^^^^^^^" << endl;
					for(int i=0;i<256;i++){
						if(i != globalMyID && i != heardFrom && i != msg->from){
							sendto(globalSocketUDP, msg, sizeof(*msg), 0,
									(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
						}
					}
					// sequence_number++;
					// forwardLSA();
					// printGraph();

				}


			pthread_mutex_unlock(&forwardLSALock);
		}else if(!strncmp(msg->type, "htbt", 4)){
			// if(neighbors[msg->from] == 0){
			// 	cout << "1st time recv htbt from node " << msg->from << endl;
			// 	// forwardLSA();
			// 	// cout << "heartBeat from: " << msg->from << endl;
			// 	// cout << "!!!!!!!!!!!!!!!!!!!!!!!!loop in htbt!!!!!!!!!!!!!!!!!!!!!!!" << endl;
			// 	neighbors[msg->from] = 1;
			// 	// cout << "node " << msg->from << " is online" << endl;
			// 	// for(int i = 0; i < 256; i++){
			// 	// 	cost[msg->from][i] = msg->lsaContent[i];
			// 	// 	cost[i][msg->from] = msg->lsaContent[i];
			// 	// }
			//
			// 	cost[msg->from][globalMyID] = neighbors_cost[msg->from];
			// 	cost[globalMyID][msg->from] = neighbors_cost[msg->from];
			//
			// 	if(cost[msg->from][globalMyID] <= 0){
			// 		cost[msg->from][globalMyID] = 1;
			// 		cost[globalMyID][msg->from] = 1;
			// 	}
			// 	printGraph();
			// }


				update *updt = (update *)msg;
				// memset(updt, 0, sizeof(*updt));
				char *token;
				// cout << "recv update from node " << updt->nodeID << endl;
				token = strtok(updt->lsaContent, ",");
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
				// flag = 1;
				// printGraph();




			// typedef struct Update{
			// 	char type[4];
			// 	short int nodeID;
			// 	char lsaContent[200];
			// }update;




			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
		}else if(!strncmp(msg->type, "updt", 4)){

			if(flag == 0){


				update *updt = (update *)msg;
				// memset(updt, 0, sizeof(*updt));
				char *token;
				cout << "recv update from node " << updt->nodeID << endl;
				token = strtok(updt->lsaContent, ",");
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
				flag = 1;
				printGraph();
			}
		}else if(!strncmp(msg->type, "join", 4)){
			cout << "node " << msg->from << " just join the Net" << endl;
			cout << "gonna sent netinfo to node " << msg->from << endl;
			// sleep(1);
			update *updt = new update;
			memset(updt, 0, sizeof(*updt));
			strcpy(updt->type, "updt");
			updt->nodeID = globalMyID;
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
			strcpy(updt->lsaContent, st1);
			cout << updt->lsaContent << endl;
			sendto(globalSocketUDP, updt, sizeof(*updt), 0,	(struct sockaddr*)&globalNodeAddrs[msg->from], sizeof(globalNodeAddrs[msg->from]));
			// printGraph();

		}
		//TODO now check for the various types of packets you use in your own protocol
		//else if(!strncmp(recvBuf, "your other message types", ))
		// ...
	}
	//(should never reach here)
	close(globalSocketUDP);
}
