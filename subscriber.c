#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "helpers.h"

/* Extracts the number of arguments received to ensure that they are enough;
for example, the subscribed function is not called only with one arg(causing seg fault) */
int number_of_arguments(char* buf) {
	int number_of_arguments = 1;
	int len = strlen(buf);
	for(int i = 0; i < len; i++) {
		if(buf[i] == ' ')
			number_of_arguments++;
	}
	return number_of_arguments;
}

void usage(char *file) {
	/*The usage of the clientl, that receives from stdin his id, the server's ip
	and the server's port */
	fprintf(stderr, "Usage: %s <ID_Client> <IP_Server> <Port_Server>\n", file);
    exit(0);
}

int main(int argc, char *argv[]) {

	//Insufficient number of args
    if (argc < 4) {
        usage(argv[0]);
    }

    struct sockaddr_in serv_addr;
    char buffer[BUFLEN];

    //flushing the buffer
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int sockfd, ret;

    //Completes the serv_addr structury with ip, port and family
    ret = inet_aton(argv[2], &serv_addr.sin_addr);
    DIE(ret == 0, "inet_aton");
    int host_port = atoi(argv[3]);
    serv_addr.sin_port = htons(host_port);
    serv_addr.sin_family = AF_INET;

    //Initialises two sets of sockets
    fd_set read_file_descriptors;
    FD_ZERO(&read_file_descriptors);

    fd_set tmp_file_descriptors;
    FD_ZERO(&tmp_file_descriptors);

    //Opens the listen socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    ret = connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "connect");

    //Client sends his ID to the server, to verify uniqueness
    int n = send(sockfd, argv[1], strlen(argv[1])+ 1, 0);
    DIE(n < 0, "ID checking");

    /* Both the listen socket and the stdin file descriptor are added to the set,
    to allow multiplexing */
    FD_SET(sockfd, &read_file_descriptors);
    FD_SET(0, &read_file_descriptors);

    //The Neagle algorithm is disabled to prevent packets from overlapping
    char flag = 1;
    int result = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(int));
    DIE(result < 0, "setsockopt");


    //The server verification for unique ID is received
    ret = recv(sockfd, &buffer, BUFLEN, 0);
    DIE(ret < 0, "id check");

    //If the message received is 'error', then the ID is not unique
    if(!strcmp("error", buffer)) {
        flag = 0;
        printf("This ID is already used\n");
    }

    while (flag) {
        tmp_file_descriptors = read_file_descriptors;
    	int max_select = sockfd + 1;

        ret = select(max_select, &tmp_file_descriptors, NULL, NULL, NULL);
        DIE(ret < 0, "select");

        //The 0 socket is selected, so the client receives a command from stdin
        if (FD_ISSET(0, &tmp_file_descriptors)) {
            fgets(buffer, BUFLEN - 1, stdin);

            char* aux_buffer = strdup(buffer);
			char* token = strtok(aux_buffer, " \n");
			TTCP_Msg tcp_msg;

			if(token == NULL) {
			}
            else if (!strcmp(token, "exit")) {
                break;
            }else if(!strcmp(token, "subscribe") && number_of_arguments(buffer) == 3) {
		    		/* The topic, the store forward parameter and the message types
		    		are extracted */
				token = strtok(NULL, " \n");
				char* topic = token;
				
				token = strtok(NULL, " \n");
				unsigned int SF = atoi(token);
				
				tcp_msg.type = 1;
				strcpy(tcp_msg.topic, topic);
				tcp_msg.SF = SF;
				
				//The subscribe request is sent to the server
				ret = send(sockfd, &tcp_msg, sizeof(TTCP_Msg), 0);
				DIE(ret < 0, "subscribe message");
			    printf("Subscribed to topic.\n");
            } else if(!strcmp(token, "unsubscribe") && number_of_arguments(buffer) == 2) {
            	//Same process for unsubscribing
				token = strtok(NULL, " \n");
				char* topic = token;
				
				tcp_msg.type = 0;
				strcpy(tcp_msg.topic, topic);
				tcp_msg.SF = -1;
				
				ret = send(sockfd, &tcp_msg, sizeof(TTCP_Msg), 0);
				DIE(ret < 0, "unsubscribe message");
                printf("Unsubscribed from topic.\n");
            } else {
            }
            free(aux_buffer);
        }
        //The server sent the client a message:
        if (FD_ISSET(sockfd, &tmp_file_descriptors)) {
			//Most frequently, the message will be an UDP one
			TUDP_Msg udp_msg;
			int m = recv(sockfd, &udp_msg, sizeof(udp_msg), 0);
			DIE(m < 0, "recv");
 		
 			/*Otherwise, the server sends 'exit' and demands the client
			to close */
            if(!strncmp((char*)&udp_msg, "exit", 4)) {
                close(sockfd);
                return 0;
            }

            //Message of type STRING
            if(udp_msg.data_type == 3) {
				//Afisez mesajul asa cum este
				printf("%s:%d - %s - STRING - %s\n", inet_ntoa(udp_msg.ip),
				ntohs(udp_msg.port), udp_msg.topic, udp_msg.content);
            } else if(udp_msg.data_type == 2) {
            	//Type FLOAT, I extract the sign bit, exponent and mantissa
				int sign_bit = udp_msg.content[0] == 1 ? - 1 : 1;
				int32_t integer_val = ntohl(*((uint32_t*)(&udp_msg.content[1])));
				int8_t power = udp_msg.content[5];
				double float_val = (double)integer_val;
				
				while(power > 0) {
					float_val /= 10;
					power--;
				}
				
				float_val *= sign_bit;
				printf("%s:%d - %s - FLOAT - %f\n", inet_ntoa(udp_msg.ip),
				ntohs(udp_msg.port), udp_msg.topic, float_val);
            } else if(udp_msg.data_type == 1) {
				uint16_t short_int = ntohs(*((uint16_t*)(udp_msg.content)));
				double short_real = ((double)short_int) / 100;
		    		//Prints only the first 2 decimals
				printf("%s:%d - %s - SHORT_REAL - %.2f\n", inet_ntoa(udp_msg.ip),
				ntohs(udp_msg.port), udp_msg.topic, short_real);
            } else if(udp_msg.data_type == 0) {
        		int sign_bit = udp_msg.content[0] == 1 ? -1 : 1;
				int32_t integer_val = ntohl(*((uint32_t*)(&udp_msg.content[1])));
				integer_val *= sign_bit;
				printf("%s:%d - %s - INT - %d\n", inet_ntoa(udp_msg.ip),
				ntohs(udp_msg.port), udp_msg.topic, integer_val);
            } else {
				/*The message has an unknown datatype, so the client prints
				an error message */
		}
        } 
    }

    //Inchid clientul
    close(sockfd);
    return 0;
}

