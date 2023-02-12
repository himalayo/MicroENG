typedef struct vec {
	void* data;
	size_t value_size;
	size_t length;
	size_t capacity;
} vector;

vector* vector_init(size_t val_size);
void vector_push(vector* vec, void* input);
void vector_del(vector* vec, size_t index);
void vector_clear(vector* vec);
void vector_insert(vector* vec, size_t index, void* input);
void* vector_get(vector* vec, size_t index);
void* vector_last(vector* vec);
void* vector_pop(vector* vec);
void vector_module_test();
