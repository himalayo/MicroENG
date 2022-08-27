typedef struct hash_table_entry {
	int hash;
	void* data;
	size_t size;
} hash_table_entry;

typedef struct hash_table {
	hash_table_entry* entries;
	unsigned int len;
} hash_table;

hash_table* hash_table_init(unsigned int);
void* hash_table_get(hash_table*, char* str);
void hash_table_set(hash_table*,char*,void*,size_t);
void hash_table_unset(hash_table*,char*);
void hash_table_module_test();
