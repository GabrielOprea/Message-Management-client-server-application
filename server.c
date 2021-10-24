#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"
#include "table.h"
#include <netinet/tcp.h>

#define TOPIC_SIZE 50


//Modul de utilizare al serverului
void usage(char *file)
{
	fprintf(stderr, "Usage: %s <Port_Server>\n", file);
	exit(0);
}

//Functie ce scoate clientul cu socketul sock din lista de clienti
void remove_client(int* count, TClient* clients, int sock) {
	for(int i = 0; i < (*count); i++) {
		if(clients[i].sockfd == sock) {
			printf("Client %s disconnected.\n", clients[i].id);

			TClient aux = clients[i];
			clients[i] = clients[(*count) - 1];
			clients[(*count) - 1] = aux;
			(*count)--;
		}
	}
}

//Functie ce dezactiveaza algoritmul lui Nagle
void turn_off_nagle(int sockfd) {
	int flag = 1;
	int ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,
	(char*)&flag, sizeof(int));
	DIE(ret < 0, "setsockopt");
}

/*Functie ce primeste un mesaj UDP de la sockfd si il sotcheaza corespunzator
in structura TUDP_Msg */
void receive_udp(TUDP_Msg* udp_msg, int sockfd, struct sockaddr_in* cli_addr,
socklen_t* clilen) {
	memset(udp_msg, 0, sizeof(TUDP_Msg));

	int ret = recvfrom(sockfd, udp_msg, sizeof(TUDP_Msg), 0, 
						(struct sockaddr*) cli_addr, clilen);
	DIE(ret < 0, "UDP receive");


	udp_msg->port = cli_addr->sin_port;
	udp_msg->ip = cli_addr->sin_addr;
}

//Functie ce cauta clientul cu id-ul din buffer in vector si inchide socketul 
int search_client(int nr_clients, TClient* vec, char buffer[BUFLEN], int sock){
	for(int i = 0; i < nr_clients; i++) {
		if(!strcmp (buffer, vec[i].id)) {
			char msg[BUFLEN];
			printf("Client %s already connected.\n", buffer);
			sprintf(msg, "error");
			int ret = send(sock, msg, BUFLEN, 0);
			DIE(ret < 0, "send");

			close(sock);
			return 1;
		}
	}
	return 0;
}

int recv_exit(int fdmax, fd_set* read_fds, int sockfd_udp, int sockfd_tcp);

void recv_tcp_msg(int sockfd_tcp, int* fdmax, fd_set* read_fds, int* count,
int* clients_capacity, TClient** clients, int backup_count, TBackup* backup,
struct sockaddr_in* tcp_cli_addr, socklen_t* tcpclilen);

void recv_client_msg(int fdmax, fd_set* tmp_fds, fd_set* read_fds,
TClient* clients, int* count, TTable* table);

void update_table(TTable* table, TUDP_Msg* udp_msg, fd_set* read_fds,
TBackup** backup, int* backup_count, int* backup_capacity);

int main(int argc, char *argv[])
{

	//Numar insuficient de argumente
	if (argc < 2) {
		usage(argv[0]);
	}

	//Initializez tabela in care tin datele(ArrayList de liste de topicuri)
	TTable* table = init_table();
	if(!table) {
		perror("Eroare la alocare!");
		exit(1);
	}

	int count = 0; //nr curent de clienti
	int clients_capacity = 1; //capacitatea vectorului de clienti
	int backup_count = 0; //nr curent de backup-uri
	int backup_capacity = 1; //capacitatea backup-urilor

	TClient* clients = calloc(clients_capacity, sizeof(TClient));
	if(!clients) {
		perror("Eroare la alocare");
		exit(1);
	}

	TBackup* backup = calloc(backup_capacity, sizeof(TBackup));
	if(!backup) {
		perror("Eroare la alocare!");
		exit(1);
	}

	fd_set read_fds;
	fd_set tmp_fds;
	int fdmax;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	int sockfd_tcp, sockfd_udp;
	socklen_t tcpclilen;
	socklen_t udpclilen;
	struct sockaddr_in serv_addr, tcp_cli_addr, udp_cli_addr;

	int portno;
	int ret;
	int size = sizeof(struct sockaddr);
			
	portno = atoi(argv[1]);
	if(portno <= 0) {
		perror("Bad port");
		exit(1);
	}

	//Completez structura pentru adresa clientului udp
	udp_cli_addr.sin_port = htons(portno);
	udp_cli_addr.sin_family = AF_INET;
	udp_cli_addr.sin_addr.s_addr = INADDR_ANY;
	udpclilen = sizeof(struct sockaddr_in);

	//Deschid socket de listen pt udp
	sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(sockfd_udp < 0, "udp sock");

	serv_addr.sin_port = htons(portno);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	//Socket de listen pentru tcp
	sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd_tcp < 0, "tcp sock");

	//Dau bind celor 2 socketi
	ret = bind(sockfd_tcp, (struct sockaddr *) &serv_addr, size);
	DIE(ret < 0, "tcp binding failed");

	ret = bind(sockfd_udp, (struct sockaddr *) &udp_cli_addr, size);
	DIE(ret < 0, "udp binding failed");

	ret = listen(sockfd_tcp, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	//Adaug socketii in multime
	FD_SET(0, &read_fds);
	FD_SET(sockfd_tcp, &read_fds);
	FD_SET(sockfd_udp, &read_fds);
	fdmax = sockfd_tcp > sockfd_udp ? sockfd_tcp : sockfd_udp;

	//Dezactivez algoritmul Nagle
	turn_off_nagle(sockfd_tcp);
	
	tcpclilen = sizeof(tcp_cli_addr);

	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		if(ret < 0) {
			perror("select error");
			exit(-1);
		}

		//Aici serverul primeste exit de la tastatura
		if(FD_ISSET(0, &tmp_fds)) {

			ret = recv_exit(fdmax, &read_fds, sockfd_udp, sockfd_tcp);
			if(!ret)break;
		}

		//Primeste un mesaj udp
		else if (FD_ISSET(sockfd_udp, &tmp_fds)) {
			TUDP_Msg udp_msg;
			receive_udp(&udp_msg, sockfd_udp, &udp_cli_addr, &udpclilen);

			update_table(table, &udp_msg, &read_fds, &backup, &backup_count, &backup_capacity);
		}

		//Aici primesc un mesaj de pe socketul de listen tcp
		else if(FD_ISSET(sockfd_tcp, &tmp_fds)) {
			
			recv_tcp_msg(sockfd_tcp, &fdmax, &read_fds, &count, &clients_capacity, &clients, backup_count, backup, &tcp_cli_addr, &tcpclilen);

		} else {
			/* S-au primit date de la unul din socketii de client, asa
			ca iterez prin ei sa il gasesc pe cel adecvat */
			recv_client_msg(fdmax, &tmp_fds, &read_fds, clients, &count, table);		
		}
	}

	//Inchid socketii
	for(int i = 0; i<= fdmax; i++) {
		if (FD_ISSET(i, &read_fds)) {
			close(i);
		}
	}
	close(sockfd_tcp);
	close(sockfd_udp);

	for(int i = 0; i < table->crt_len; i++) {
		TTopic crt = table->array[i];
		//Eliberez lista de clienti din fiecare topic
		while(tail(crt.clients));
	} free(table->array);
	free(table);
	free(clients);

	for(int i = 0; i < backup_count; i++) {
		//Eliberez lista de mesaje pt fiecare intrare
		while(tail_UDP(backup[i].msg));
	}
	free(backup);

	return 0;
}


int recv_exit(int fdmax, fd_set* read_fds, int sockfd_udp, int sockfd_tcp) {
	char buffer[BUFLEN];
	scanf("%s", buffer);
	if(!strcmp(buffer, "exit")) {
		for(int i = 1; i <= fdmax; i++) {
			if(i != sockfd_udp && i != sockfd_tcp) {
				if(FD_ISSET(i, read_fds)) {
					int ret = send(i, buffer, BUFLEN, 0);
					DIE(ret < 0, "send exit");
					close(i);
				}
			}
		}
		return 0;
	} else {
		//Se poate afisa un mesaj de eroare
		return 1;
	}
}

void recv_tcp_msg(int sockfd_tcp, int* fdmax, fd_set* read_fds, int* count,
int* clients_capacity, TClient** clients, int backup_count, TBackup* backup,
struct sockaddr_in* tcp_cli_addr, socklen_t* tcpclilen) {
	
	char buffer[BUFLEN];

	int newsockfd = accept(sockfd_tcp, (struct sockaddr *) tcp_cli_addr, tcpclilen);
	if(newsockfd < 0) {
		perror("Cannot accept!");
		exit(-1);
	}

	//Buffer va avea id-ul clientului care vrea sa se conecteze
	int ret = recv(newsockfd, buffer, BUFLEN, 0);
	if(ret < 0) {
		perror("Recv failed");
		exit(-1);
	}

	//Verific daca e unic clientul, daca nu ignor cererea
	if(search_client(*count, *clients, buffer, newsockfd))
		return;

	//Ii adaug un mesaj de succes clientului
	char msg[BUFLEN];
	sprintf(msg, "success");
	ret = send(newsockfd, msg, BUFLEN, 0);
	DIE(ret < 0, "send");

	*fdmax = newsockfd > *fdmax ? newsockfd : *fdmax;

	//Construiesc structura TClient si o adaug in vector
	TClient cli;
	cli.sockfd = newsockfd;
	cli.SF = 0;
	memcpy(cli.id, buffer, 10);
	(*clients)[(*count)++] = cli;

	turn_off_nagle(newsockfd);
	FD_SET(newsockfd, read_fds);

	//Realoc daca este necesar
	if(*count == *clients_capacity) {
		*clients_capacity *= 2;
		TClient* aux = realloc(*clients, *clients_capacity * sizeof(TClient));
		if(aux) *clients = aux;
	}

	//Afisez faptul ca a fost conectat clientul
	printf("New client %s connected from %s:%d.\n",
			buffer, inet_ntoa(tcp_cli_addr->sin_addr),
			ntohs(tcp_cli_addr->sin_port));

	//Caut sa vad daca avea mesaje salvate
	int found = 0, k;
	for(k = 0; k < backup_count; k++) {
		if(!strcmp(backup[k].id, cli.id)) {
			found = 1;
			break;
		}
	}
	//Daca are, atunci i le trimit acum
	if(found) {
		TList_UDP p = backup[k].msg;
		while(p != NULL) {
			TUDP_Msg udp_msg = p->info;
			ret = send(newsockfd, &udp_msg, sizeof(TUDP_Msg), 0);
			DIE(ret < 0, "send backup");
			p = p->next;
		}
		backup[k].msg = NULL;
	}
}


void recv_client_msg(int fdmax, fd_set* tmp_fds, fd_set* read_fds,
TClient* clients, int* count, TTable* table) {

	for(int i = 1; i <= fdmax; i++) {
		if(FD_ISSET(i, tmp_fds)) {
			TTCP_Msg* tcp_msg = calloc(1, sizeof(TTCP_Msg)); 
			int n = recv(i, tcp_msg, sizeof(TTCP_Msg), 0);
			if(n < 0) {
				perror("Cannot receive message");
			}

			//Daca avem deconectarea clientului
			if (!n) {
				//Scot clientul din vector si ii inchid socketul
				remove_client(count, clients, i);
				close(i);
				FD_CLR(i, read_fds);
			} else {
				/*Preiau clientul care a trimis cererea de subscribe
				sau unsubscribe */
				TClient* sender = calloc(1, sizeof(TClient));
				for(int j = 0; j < *count; j++) {
					if(clients[j].sockfd == i) {
						memcpy(sender, &clients[j], sizeof(TClient));
					}
				}
				//Avem subscribe
				if(tcp_msg->type == 1) {
					sender->SF = tcp_msg->SF;

					int flag = 0;
					for(int j = 0; j < table->crt_len; j++) {
						//Caut topicul cerut in table
						if(!strncmp(table->array[j].topic, tcp_msg->topic, TOPIC_SIZE)) {
							flag = 1;
							TList p = table->array[j].clients;
							TList dest = NULL;
							/*Fac o inserare la inceput de lista, daca
							nu exista clientul */
							while(p != NULL) {
								if(!strcmp(p->info.id, sender->id)) {
									dest = p;
									break;
								}
								p = p->next;
							}
							if(!dest) {
								table->array[j].clients = cons(*sender, table->array[j].clients);
							} else {
								//Daca exista, ii updatez SF-ul cu cel nou
								dest->info.SF = tcp_msg->SF;
							} break;
						}
					} if(!flag) {
							//Daca table nu avea topicul cerut, il creez
							TTopic t = create_topic(tcp_msg->topic);
							t.clients = cons(*sender, t.clients);
							add_topic(table, t);
						}
				//Cazul in care da unsubscribe
				} else if(tcp_msg->type == 0) {
					for(int j = 0; j < table->crt_len; j++) {
						//Caut topicul de la care vrea sa se dezaboneze
						if(!strcmp(table->array[j].topic, tcp_msg->topic)) {
							TList p = table->array[j].clients;
							TList crt = NULL;
							TList ant = NULL;
							//Fac o eliminare din linked list
							while(p != NULL) {
								if(!strcmp(p->info.id, sender->id)) {
									crt = p;
									break;
								}
								ant = p;
								p = p->next;
							}
							if(crt != NULL) {
								//Cazul in care nu e primul element
								if(ant != NULL)
									ant->next = crt->next;
								//Cazul in care e primul element
								else table->array[j].clients = table->array[j].clients->next;
							}
							break;
						}
					}
				}
			}
			free(tcp_msg);
		}
	}
}

void update_table(TTable* table, TUDP_Msg* udp_msg, fd_set* read_fds,
TBackup** backup, int* backup_count, int* backup_capacity) {
	for(int j = 0; j < table->crt_len; j++) {
		//Caut topicul in table
		if(!strncmp(table->array[j].topic, udp_msg->topic,TOPIC_SIZE)) {
			TList p = table->array[j].clients;
			//Daca l-am gasit, parcurg lista de clienti abonati
			while(p != NULL) {
				int sock = p->info.sockfd;
				if(FD_ISSET(sock, read_fds)) {
					//Daca este inca conectat, ii trimit mesajul
					int ret = send(sock, udp_msg, sizeof(TUDP_Msg), 0);
					DIE(ret < 0, "send");
				} else if(p->info.SF == 1) {
					/*Daca nu e conectat dar are store-forward,il adaug
					in backup-ul rezervat lui */
					int found = 0;
					for(int k = 0; k < *backup_count; k++) {
						if(!strcmp((*backup)[k].id, p->info.id)) {
							(*backup)[k].msg = cons_UDP(*udp_msg, (*backup)[k].msg);
							found = 1;
							break;
						}
					}
					//Daca nu are backup rezervat, ii fac unul
					if(!found) {
						(*backup)[*backup_count] = init_backup(p->info);
						//Adaug mesajul nou
						(*backup)[*backup_count].msg = cons_UDP(*udp_msg, (*backup)[*backup_count].msg);
						(*backup_count)++;
						//Realoc vectorul daca e necesar
						if(*backup_count == *backup_capacity) {
							*backup_capacity *= 2;
							TBackup* aux = realloc(*backup, *backup_capacity * sizeof(TBackup));
							if(aux) *backup = aux;
						}
					}
				}
				p = p->next;
			}
		}
	}
}
