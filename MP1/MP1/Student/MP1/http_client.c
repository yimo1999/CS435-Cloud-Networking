/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "80"
#define MAXDATASIZE 1024// max number of bytes we can get at once


void output(char* str){
	FILE *fp;
	fp = fopen("output", "w+");
	if(fp==NULL)
	{
	printf("File cannot open!" );
	exit(1);
	}

	fprintf(fp, "%s", str);
	fclose(fp);

	return ;
}

int isVaild(char* str){
	char *c;
	c = strstr(str, "http:");
	// printf("%d\n", strlen(buf));
//    printf("%d\n", c == NULL);
	if(c == NULL || strlen(str) > strlen(c)){
		char alert[] = "INVALIDPROTOCOL";
		output(alert);
		exit(1);

	}
	return 1;
}

void split(char* str, char* host, char* port, char* dir){

	char *token;
	token = strtok(str, "/");
	int idx = 0;
	char hostPort[100];
	char director[512];

	while(token != NULL){

		if(idx == 0){
			isVaild(token);

		}
		else if(idx == 1){
			strcpy(hostPort, token);
			// printf("%s------------\n", hostPort);

		}
		else{
			if(idx == 2){
				strcpy(director, "/");
			}
			else{
				strcat(director, "/");
			}

			strcat(director, token);

		}
		idx ++;


		token = strtok(NULL, "/");
	}

	strcpy(dir, director);
	strcat(dir, "\0");

  idx = 0;


	token = strtok(hostPort, ":");

	while(token != NULL){
		if(idx == 0){
			strcpy(host, token);


		}else if(idx == 1){
			strcpy(port, token);

		}
		idx ++;

		token = strtok(NULL, ":");
	}
	if(idx == 1){
		strcpy(port, PORT);
	}


	// printf("%s", port);
	return;


}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	char host[100];
	char port[100];
	char dir[512];
	char httpRequest[200];


	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

 	// printf("---%s---\n", argv[1]);
	split(argv[1], host, port, dir);

	if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
		output("NOCONNECTION");
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}


	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}



	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		output("NOCONNECTION");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure
	// printf("%s", dir);
	strcpy(httpRequest, "GET ");
	strcat(httpRequest, dir);
	strcat(httpRequest, " HTTP/1.0\r\n\r\n");
	// GET /example_file HTTP/1.0\r\n\r\n
	// GET /example_file HTTP/1.0\r\nUser-Agent: Wget/1.15 (linux-gnu)\r\n Accept: */*\r\n Connection: Keep-Alive\r\n\r\n
	// strcpy(httpRequest, "User-Agent: Wget/1.15 (linux-gnu)\r\n");
	// strcpy(httpRequest, "Accept: */*\r\n");
	// strcpy(httpRequest, "Connection: Keep-Alive\r\n\r\n");


  // printf("---------%s------\n", host);

	// printf("--------%s----\n", port);
	// printf("%s\n~~~~~~~~~~~~~~~~~~~", httpRequest);
	if(send(sockfd, httpRequest, strlen(httpRequest), 0) <= 0){
			perror("send");
			exit(1);
	}


	FILE *fp;
	fp = fopen("output", "wb");
	if(fp==NULL)
	{
	printf("File cannot open! " );
	exit(0);
	}

	// fprintf(fp, "%s", buf);


	int flag = 0;
	while((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) > 0) {


			// char *data = strstr(buf, "\r\n\r\n");
			// printf("%s", data);

			// printf("%s", buf);]
			if(strstr(buf, "404 File not found") || strstr(buf, "Error code: 404")){
				output("FILENOTFOUND");
			}

			if(flag == 0){
				char *data = strstr(buf, "\r\n\r\n");
				if(data){
					data = data + 4;
					fwrite(data, sizeof(char) - 1, sizeof(data) - 1, fp);
					flag = 1;
				}
				memset(data, 0, sizeof(data));
			}
			else if(flag == 1){
				fwrite(buf, sizeof(char), numbytes, fp);
				// fprintf(fp, "%s", buf);
			}


			memset(buf, 0, sizeof(buf));


			// printf("%s",buf);
	}
	fclose(fp);
	close(sockfd);
	// buf[numbytes] = '\0';
	// printf("%x",buf[0]);

	// printf("client: received '%s'\n",buf);



	return 0;
}
