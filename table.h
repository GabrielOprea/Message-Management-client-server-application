#include "helpers.h"
#include <stdlib.h>
#include <string.h>

TBackup init_backup(TClient cli);
TList cons(TClient element, TList l);
TList tail(TList l);
TList_UDP cons_UDP(TUDP_Msg element, TList_UDP l);
TList_UDP tail_UDP(TList_UDP l);
TTable* init_table();
void add_topic(TTable* table, TTopic topic);
TTopic create_topic(char* topic);
void print_table(TTable* table);
void print_backup(TBackup* backup, int backup_count);
