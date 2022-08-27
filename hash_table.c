#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "hash_table.h"

#define HASH_TABLE_SIZE 255

unsigned int ELFHash(char* str, unsigned int length) {
	unsigned int hash = 0;
	unsigned int x = 0;
	unsigned int i = 0;

	for (i = 0; i < length; str++, i++)
	{
		hash = (hash << 4) + (*str);

		if ((x = hash & 0xF0000000L) != 0)
		{
			hash ^= (x >> 24);
		}

		hash &= ~x;
	}

	return hash;
}

hash_table*
hash_table_init(unsigned int size)
{
	if (size == 0)
	{
		size = HASH_TABLE_SIZE;
	}
	hash_table* out = malloc(sizeof(hash_table));
	out->len = size;
	out->entries = malloc(sizeof(hash_table_entry)*out->len);
	for (int i=0; i<out->len; i++)
	{
		out->entries[i] = (hash_table_entry){.hash=0,.data=NULL,size=0};
	}
	return out;
}

void*
hash_table_get(hash_table* table, char* str)
{	
	unsigned int hash = ELFHash(str,(unsigned int)strlen(str));
	
	hash_table_entry entry = table->entries[hash%table->len];
	if (entry.hash != hash)
		return NULL;
	
	return entry.data;
}

void
hash_table_set(hash_table* table, char* str, void* data, size_t size)
{
	unsigned int hash = ELFHash(str,(unsigned int)strlen(str));
	unsigned int index = hash%table->len;
	if (table->entries[index].size != size)
	{
		table->entries[index].data = realloc(table->entries[index].data,size);
		table->entries[index].size = size;
	}
	table->entries[index].hash = hash;
	memcpy(table->entries[index].data,data,size);
}

void
hash_table_unset(hash_table* table, char* str)
{
	unsigned int hash = ELFHash(str,(unsigned int)strlen(str));
	
	free(table->entries[hash%table->len].data);
	table->entries[hash%table->len].size = 0;
	table->entries[hash%table->len].hash = 0;
}

void
hash_table_module_test()
{
	char temp_buff[50];
	hash_table* table = hash_table_init(0);
	int test = 10;
	hash_table_set(table,"test\0",&test,sizeof(int));
	write(STDOUT_FILENO,temp_buff,snprintf(temp_buff,sizeof(temp_buff),"Expected: %d\nGot: %d\n\n",test,*(int*)hash_table_get(table,"test\0")));
}
