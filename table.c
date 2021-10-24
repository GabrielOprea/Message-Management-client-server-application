#include "helpers.h"
#include <stdlib.h>
#include <string.h>

//Initializeaza un backup pt client
TBackup init_backup(TClient cli) {
	TBackup backup;
	memcpy(backup.id, cli.id, 10);
	backup.msg = NULL;
	return backup;
}

//Adauga la inceputul listei de clienti
TList cons(TClient element, TList l)
{
	TList temp = malloc(sizeof(TCell));
	if(!temp) return l;
	temp->info = element;
	temp->next = l;
	return temp;
}

//Adauga la inceputul listei de mesaje udp din backup
TList_UDP cons_UDP(TUDP_Msg element, TList_UDP l)
{
	TList_UDP temp = malloc(sizeof(TCell_UDP));
	if(!temp) return l;
	temp->info = element;
	temp->next = l;
	return temp;
}

//Elibereaza un element din lista de clienti 
TList tail(TList l)
{
	TList temp = l->next; 
	free(l);
	return temp;
}

//Elibereaza un element din lista de mesaje
TList_UDP tail_UDP(TList_UDP l)
{
	TList_UDP temp = l->next; 
	free(l);
	return temp;
}

//Intoarce primul mesaj udp
TUDP_Msg head_UDP(TList_UDP l) {
	return l->info;
}

/*Initiaza o tabela in care stochez un vector de topicuri, iar fiecare
topic va avea o lista simplu inlantuita de clienti */
TTable* init_table() {
	TTable* temp = malloc(sizeof(TTable));
	if(!temp) return NULL;
	temp->array = calloc(1, sizeof(TTopic));
	temp->capacity = 1;
	temp->crt_len = 0;

	return temp;
}

//Adauga un nou topic in arraylist si realoca memorie, dupa caz
void add_topic(TTable* table, TTopic topic) {
	table->array[table->crt_len] = topic;
	table->crt_len++;
	if(table->crt_len == table->capacity) {
		table->capacity *= 2;
		table->array = realloc(table->array, table->capacity * sizeof(TTopic));
	}
}

//Creaza un topic cu un anumit continut in numele sau
TTopic create_topic(char* topic) {
	TTopic t;
	memcpy(t.topic, topic, 50 * sizeof(char));
	t.clients = NULL;
	return t;
}

//Afiseaza tabela, utilizat pentru debugging
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

//Afiseaza backup-ul, utilizat pentru debugging
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