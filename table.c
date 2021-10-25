#include "helpers.h"
#include <stdlib.h>
#include <string.h>

//Initialise backup for a client
TBackup init_backup(TClient cli) {
	TBackup backup;
	memcpy(backup.id, cli.id, 10);
	backup.msg = NULL;
	return backup;
}

//Adds a client at the beginning of the list
TList cons(TClient element, TList l)
{
	TList temp = malloc(sizeof(TCell));
	if(!temp) return l;
	temp->info = element;
	temp->next = l;
	return temp;
}

//Adds at the beginning of the UDP list of messages
TList_UDP cons_UDP(TUDP_Msg element, TList_UDP l)
{
	TList_UDP temp = malloc(sizeof(TCell_UDP));
	if(!temp) return l;
	temp->info = element;
	temp->next = l;
	return temp;
}

//Deletes and frees an element from the clients' list.
TList tail(TList l)
{
	TList temp = l->next; 
	free(l);
	return temp;
}


//Deletes and frees an element from the messages' list.
TList_UDP tail_UDP(TList_UDP l)
{
	TList_UDP temp = l->next; 
	free(l);
	return temp;
}

//Returns the first UDP message
TUDP_Msg head_UDP(TList_UDP l) {
	return l->info;
}

/*Initialises a table with an array of topics, each topic having a
singly linked list of clients */
TTable* init_table() {
	TTable* temp = malloc(sizeof(TTable));
	if(!temp) return NULL;
	temp->array = calloc(1, sizeof(TTopic));
	temp->capacity = 1;
	temp->crt_len = 0;

	return temp;
}

//Adds a new topic in the arraylist and does memory reallocation if necessary
void add_topic(TTable* table, TTopic topic) {
	table->array[table->crt_len] = topic;
	table->crt_len++;
	if(table->crt_len == table->capacity) {
		table->capacity *= 2;
		table->array = realloc(table->array, table->capacity * sizeof(TTopic));
	}
}

//Creates a new topic with the name from the parameter
TTopic create_topic(char* topic) {
	TTopic t;
	memcpy(t.topic, topic, 50 * sizeof(char));
	t.clients = NULL;
	return t;
}

//Prints the table, used for debugging
void print_table(TTable* table) {
	for(int i = 0; i < table->crt_len; i++) {
		printf("%s ", table->array[i].topic);
		TList p = table->array[i].clients;
		while(p != NULL) {
			printf("%s ", p->info.id);
			p = p->next;
		}
		printf("\n");
	}
}

//Prints the backup, used for debugging
void print_backup(TBackup* backup, int backup_count) {
	for(int k = 0; k < backup_count; k++) {
		printf("%s ", backup[k].id);
		TList_UDP p = backup[k].msg;
		while(p != NULL) {
			TUDP_Msg udp_msg = p->info;
			printf("%s\n", udp_msg.topic);
			p = p->next;
		}
	}
}
