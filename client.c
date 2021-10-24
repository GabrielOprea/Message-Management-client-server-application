#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s <ID_Client> <IP_Server> <Port_Server>\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	if (argc < 4) {
		usage(argv[0]);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	//Initializez file descriptorii, respectiv back-up ul lor
	fd_set tmp_file_descriptors;
	fd_set read_file_descriptors;

	FD_ZERO(&read_file_descriptors);
	FD_ZERO(&tmp_file_descriptors);

	FD_SET(0, &read_file_descriptors);
	FD_SET(sockfd, &read_file_descriptors);

	n = send(sockfd, argv[1], 1 + strlen(argv[1]), 0);
	DIE(n < 0, "ID check");

	while (1) {
  		// se citeste de la tastatura
  		tmp_file_descriptors = read_file_descriptors;
  		ret = select(sockfd + 1, &tmp_file_descriptors, NULL, NULL, NULL);
  		DIE(ret < 0, "select");

  		if(FD_ISSET(0, &tmp_file_descriptors)) {
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);
			char* aux_buffer = strdup(buffer);
			char* token = strtok(aux_buffer, " \n");
			TTCP_Msg* tcp_msg = calloc(1, sizeof(TTCP_Msg));

			if (strcmp(token, "exit") == 0) {
				break;
			}
			else if(!strcmp(token, "subscribe")) {
				token = strtok(NULL, " \n");
				char* topic = token;
				token = strtok(NULL, " \n");
				unsigned int SF = atoi(token);
				tcp_msg->type = 1;
				memcpy(tcp_msg->topic, topic, 50 * sizeof(char));
				tcp_msg->SF = SF;
				ret = send(sockfd, &tcp_msg, sizeof(TTCP_Msg), 0);
				DIE(ret < 0, "subscribe message");
			}

			else if(!strcmp(token, "unsubscribe")) {
				token = strtok(NULL, " \n");
				char* topic = token;
				tcp_msg->type = 0;
				memcpy(tcp_msg->topic, topic, 50 * sizeof(char));
				tcp_msg->SF = -1;
				ret = send(sockfd, &tcp_msg, sizeof(TTCP_Msg), 0);
				DIE(ret < 0, "unsubscribe message");
			} else {
				printf("Unknown client command\n");
			}

			// se trimite mesaj la server
			//n = send(sockfd, buffer, strlen(buffer), 0);
			//DIE(n < 0, "send");
		} else if (FD_ISSET(sockfd, &tmp_file_descriptors)) {
			TUDP_Msg* udp_msg = calloc(1, sizeof(TUDP_Msg));
			memset(udp_msg, 0, sizeof(TUDP_Msg));
			int m = recv(sockfd, &udp_msg, sizeof(udp_msg), 0);
			DIE(m < 0, "recv");
			//if(m != 0)
			//	fprintf(stderr, "Received: %s", buffer);
			if(!strncmp(udp_msg->topic, "exit", 4)) {
				printf("%s\n", udp_msg->topic);
				break;
			} else {
				if(udp_msg->data_type == 0) {
					int sign_bit = udp_msg->content[0] == 1 ? -1 : 1;

					int32_t integer_val = ntohl(*((uint32_t*)(&udp_msg->content[1])));
					integer_val *= sign_bit;
					printf("%s:%d - %s - INT - %d\n", inet_ntoa(udp_msg->ip),
						ntohs(udp_msg->port), udp_msg->topic, integer_val);

				} else if(udp_msg->data_type == 1) {
					uint16_t short_int = ntohs(*((uint16_t*)(&udp_msg->content[1])));
					double short_real = ((double)short_int) / 100;

					printf("%s:%d - %s - SHORT_REAL - %f\n", inet_ntoa(udp_msg->ip),
						ntohs(udp_msg->port), udp_msg->topic, short_real);

				} else if(udp_msg->data_type == 2) {
					int sign_bit = udp_msg->content[0] == 1 ? - 1 : 1;
					int32_t integer_val = ntohl(*((uint32_t*)(&udp_msg->content[1])));
					int8_t power = udp_msg->content[5];
					double float_val = (double)integer_val;
					while(power > 0) {
						float_val /= 10;
						power--;
					}
					float_val *= sign_bit;
					printf("%s:%d - %s - FLOAT - %f\n", inet_ntoa(udp_msg->ip),
						ntohs(udp_msg->port), udp_msg->topic, float_val);

				} else if(udp_msg->data_type == 3) {
					printf("%s:%d - %s - STRING - %s\n", inet_ntoa(udp_msg->ip),
						ntohs(udp_msg->port), udp_msg->topic, udp_msg->content);
				}
			}

		}
	}

	close(sockfd);

	return 0;
}
