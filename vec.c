#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vec.h"

#define INIT_SIZE 32

vector*
vector_init(size_t val_size)
{
	vector* output = malloc(sizeof(vector));
	output->data = malloc(INIT_SIZE*val_size);
	output->value_size = val_size;
	output->length = 0;
	output->capacity = 0;

	return output;
}

void
vector_push(vector* vec, void* input)
{
	if ( vec->length+1 < vec->capacity )
	{
		vec->capacity *= 2;
		vec->data = realloc(vec->data, vec->capacity*vec->value_size);
	}
	
	memcpy(vec->data+(vec->length*vec->value_size),input,vec->value_size);
	vec->length++;
}

void
vector_del(vector* vec, size_t index)
{
	void* element = vec->data+(index*vec->value_size);
	memmove(element,element+vec->value_size, (vec->length-index)*vec->value_size);
	vec->length--;
}

void
vector_clear(vector* vec)
{
	memset(vec->data,0,vec->length*vec->value_size);
	vec->length = 0;
}

void
vector_insert(vector* vec, size_t index, void* input)
{
	if ( vec->length+1 < vec->capacity )
	{
		vec->capacity *= 2;
		vec->data = realloc(vec->data, vec->capacity*vec->value_size);
	}

	memmove(vec->data+((index+1)*vec->value_size), vec->data+(index*vec->value_size), (vec->length-index)*vec->value_size);
	memcpy(vec->data+(index*vec->value_size), input, vec->value_size);
	vec->length++;
}

void*
vector_get(vector* vec, size_t index)
{
	return vec->data+(index*vec->value_size);
}

void*
vector_last(vector* vec)
{
	return vec->data+(vec->length-1)*(vec->value_size);
}

void*
vector_pop(vector* vec)
{
	void* output = malloc(vec->value_size);
	memcpy(output, vec->data+((vec->length-1)*vec->value_size), vec->value_size);
	memset(vec->data+((vec->length-1)*vec->value_size),0,vec->value_size);
	vec->length--;
	return output;
}

void
vector_module_test()
{
	int test = 6;
	vector* vec = vector_init(sizeof(int));
	vector_push(vec, &test);
	printf("\nExpected value: %d\nGot: %d\n",test,*(int*)vector_last(vec));
	
	for (int i=0; i<10; i++)
	{
		vector_push(vec,&i);
	}
	vector_del(vec,1);
	printf("\nExpected value: 2\nGot: %d\n",*(int*)vector_get(vec,1));
	test = 17;
	vector_insert(vec,5,&test);	
	printf("\nExpected value: %d\nGot: %d\n",test,*(int*)vector_get(vec,5));
}
