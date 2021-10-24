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

/*Extrage numarul de argumente primite pentru a se asigura ca sunt suficiente,
de ex nu se apeleaza subscribe doar cu 1 argument(ce ar cauza seg fault) */
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
	/*Modul de utilizare al clientului, primind din linia de comanda id-ul sau,
	ip-ul server-ului la care se conecteaza si portul sau */
	fprintf(stderr, "Usage: %s <ID_Client> <IP_Server> <Port_Server>\n", file);
    exit(0);
}

int main(int argc, char *argv[]) {

	//Nr insuficient de argu,ente
    if (argc < 4) {
        usage(argv[0]);
    }

    struct sockaddr_in serv_addr;
    char buffer[BUFLEN];

    //Dau flush pentru a putea rula corect testele automate
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int sockfd, ret;

    //Completez structura serv_addr cu adresa serverului, port si familie
    ret = inet_aton(argv[2], &serv_addr.sin_addr);
    DIE(ret == 0, "inet_aton");
    int host_port = atoi(argv[3]);
    serv_addr.sin_port = htons(host_port);
    serv_addr.sin_family = AF_INET;

    //Initializez doua set-uri de socketi
    fd_set read_file_descriptors;
    FD_ZERO(&read_file_descriptors);

    fd_set tmp_file_descriptors;
    FD_ZERO(&tmp_file_descriptors);

    //Deschid un socket de listen
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    ret = connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "connect");

    //Trimit ID-ul clientului la server, pentru verificare
    int n = send(sockfd, argv[1], strlen(argv[1])+ 1, 0);
    DIE(n < 0, "ID checking");

    /*Adaug in multime socketul de listen si descriptorul pentru stdin, pentru
    a putea multiplexa intre cele doua */
    FD_SET(sockfd, &read_file_descriptors);
    FD_SET(0, &read_file_descriptors);

    /*Dezactivarea algoritmului lui Nagle, pentru a evita suprapunerea
    pachetelor */
    char flag = 1;
    int result = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(int));
    DIE(result < 0, "setsockopt");


    //Primesc 'ACK-ul' de la server  cu privire la unicitatea ID-ului
    ret = recv(sockfd, &buffer, BUFLEN, 0);
    DIE(ret < 0, "id check");

    //Daca se primeste error inseamna ca exista deja acel ID conectat 
    if(!strcmp("error", buffer)) {
        flag = 0;
        printf("This ID is already used\n");
    }

    while (flag) {
        tmp_file_descriptors = read_file_descriptors;
    	int max_select = sockfd + 1;

        ret = select(max_select, &tmp_file_descriptors, NULL, NULL, NULL);
        DIE(ret < 0, "select");

        //Am primit ceva de la tastatura in client
        if (FD_ISSET(0, &tmp_file_descriptors)) {
            fgets(buffer, BUFLEN - 1, stdin);

            //Obtin comanda efectuata
            char* aux_buffer = strdup(buffer);
			char* token = strtok(aux_buffer, " \n");
			TTCP_Msg tcp_msg;

			if(token == NULL) {
                //Am putea afisa mesaj comanda invalida
			}
			//Inchid clientul
            else if (!strcmp(token, "exit")) {
                break;
            }else if(!strcmp(token, "subscribe") && number_of_arguments(buffer) == 3) {
				//Extract pe rand topic-ul, SF-ul, tipul mesajului
				token = strtok(NULL, " \n");
				char* topic = token;
				
				token = strtok(NULL, " \n");
				unsigned int SF = atoi(token);
				
				tcp_msg.type = 1;
				strcpy(tcp_msg.topic, topic);
				tcp_msg.SF = SF;
				
				//Trimit cererea de subscribe la server
				ret = send(sockfd, &tcp_msg, sizeof(TTCP_Msg), 0);
				DIE(ret < 0, "subscribe message");
			    printf("Subscribed to topic.\n");
            } else if(!strcmp(token, "unsubscribe") && number_of_arguments(buffer) == 2) {
            	//Analog pentru dezabonare
				token = strtok(NULL, " \n");
				char* topic = token;
				
				tcp_msg.type = 0;
				strcpy(tcp_msg.topic, topic);
				tcp_msg.SF = -1;
				
				ret = send(sockfd, &tcp_msg, sizeof(TTCP_Msg), 0);
				DIE(ret < 0, "unsubscribe message");
                printf("Unsubscribed from topic.\n");
            } else {
                //Am putea afisa mesaj comanda invalida
            }
            free(aux_buffer);
        }
        //Server-ul ii transmite un mesaj clientului
        if (FD_ISSET(sockfd, &tmp_file_descriptors)) {
			//Cel mai frecvent, mesajul va fi unul UDP
			TUDP_Msg udp_msg;
			int m = recv(sockfd, &udp_msg, sizeof(udp_msg), 0);
			DIE(m < 0, "recv");
 		
 			//Daca ii trimite exit, inseamna ca se vrea inchiderea clientului
            if(!strncmp((char*)&udp_msg, "exit", 4)) {
                close(sockfd);
                return 0;
            }

            //Tip STRING
            if(udp_msg.data_type == 3) {
				//Afisez mesajul asa cum este
				printf("%s:%d - %s - STRING - %s\n", inet_ntoa(udp_msg.ip),
				ntohs(udp_msg.port), udp_msg.topic, udp_msg.content);
            } else if(udp_msg.data_type == 2) {
            	//TIP FLOAT, extract bitul de semn, exponentul si mantisa
				int sign_bit = udp_msg.content[0] == 1 ? - 1 : 1;
				int32_t integer_val = ntohl(*((uint32_t*)(&udp_msg.content[1])));
				int8_t power = udp_msg.content[5];
				double float_val = (double)integer_val;
				//Ridic la puterea power
				while(power > 0) {
					float_val /= 10;
					power--;
				}
				//Inmultesc cu bitul de semn
				float_val *= sign_bit;
				printf("%s:%d - %s - FLOAT - %f\n", inet_ntoa(udp_msg.ip),
				ntohs(udp_msg.port), udp_msg.topic, float_val);
            } else if(udp_msg.data_type == 1) {
				uint16_t short_int = ntohs(*((uint16_t*)(udp_msg.content)));
				double short_real = ((double)short_int) / 100;
				//Afisez doar primele 2 zecimale
				printf("%s:%d - %s - SHORT_REAL - %.2f\n", inet_ntoa(udp_msg.ip),
				ntohs(udp_msg.port), udp_msg.topic, short_real);
            } else if(udp_msg.data_type == 0) {
        		int sign_bit = udp_msg.content[0] == 1 ? -1 : 1;
				int32_t integer_val = ntohl(*((uint32_t*)(&udp_msg.content[1])));
				integer_val *= sign_bit;
				printf("%s:%d - %s - INT - %d\n", inet_ntoa(udp_msg.ip),
				ntohs(udp_msg.port), udp_msg.topic, integer_val);
            } else {
            	//Mesajul are un data_type pe care clientul nu il stie inca
                //Se poate afisa un mesaj de eroare
			}
        } 
    }

    //Inchid clientul
    close(sockfd);
    return 0;
}

