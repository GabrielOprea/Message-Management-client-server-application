#ifndef _HELPERS_H
#define _HELPERS_H 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
	char topic[50];
	unsigned char data_type;
	char content[1500];
	struct in_addr ip;
	unsigned short port;
} TUDP_Msg;

typedef struct {
	char topic[51];
	unsigned int type;
	unsigned int SF;
} TTCP_Msg;

typedef struct {
	unsigned int sockfd;
	char id[10];
	unsigned int SF;
} TClient;

typedef struct list {
	TClient info;
	struct list* next;
} TCell, *TList;

typedef struct udp_list {
	TUDP_Msg info;
	struct udp_list* next;
} TCell_UDP, *TList_UDP;

typedef struct {
	char id[10];
	TList_UDP msg;
} TBackup;

typedef struct {
	TList clients;
	char topic[51];
} TTopic;

typedef struct {
	int capacity;
	int crt_len;
	TTopic* array;
} TTable;

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define BUFLEN		1000	
#define MAX_CLIENTS	50	

#endif
