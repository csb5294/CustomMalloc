#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "mymalloc.h"
#include "color.h"

// ------------------------------------------------------------------------------------------------
// Constants
// DON'T CHANGE OR REMOVE THESE.

// All allocations will be forced to be at least this many bytes.
#define MINIMUM_ALLOCATION  16

// All allocations will be rounded up to a multiple of this number.
#define SIZE_MULTIPLE       8

// Minimum size of a *block,* which is the size of the header and data portion put together.
#define MIN_BLOCK_SIZE (MINIMUM_ALLOCATION + sizeof(Header))

// Shorthand to zero out memory at a pointer. It assumes that the pointer is non-void* and
// will zero out one item at the pointer.
#define memzero(p) (memset((p), 0, sizeof(*p)))

// ------------------------------------------------------------------------------------------------
// Header struct

typedef struct Header {
	bool used;
	size_t size;
	struct Header* prev;
	struct Header* next;
} Header;

// ------------------------------------------------------------------------------------------------
// Globals

Header* heap_head;
Header* heap_tail;

// ------------------------------------------------------------------------------------------------
// Helper functions

size_t round_up_size(size_t data_size) {
	if(data_size == 0)
		return 0;
	else if(data_size < MINIMUM_ALLOCATION)
		return MINIMUM_ALLOCATION;
	else
		return (data_size + (SIZE_MULTIPLE - 1)) & ~(SIZE_MULTIPLE - 1);
}

void fatal(const char* format, ...) {
	va_list args;
	va_start(args, format);
	fprintf(stderr, CRED);
	vfprintf(stderr, format, args);
	fprintf(stderr, CRESET);
	fprintf(stderr, "\n");
	va_end(args);
	exit(1);
}

static void node_link(Header* n, Header* after) {
	
	if(n == NULL)
		return;

	if(after == NULL){
		heap_head = n;
		heap_tail = n;
		return;
	}
	
	if(after->next == NULL){
		n->prev = after;
		after->next = n;
		n->next = NULL;
		heap_tail = n;
		return;
	}
	
	n->next = after->next;
	n->prev = after;
	after->next->prev = n;
	after->next = n;
}

static void node_unlink(Header* n) {

	if(n == NULL)
		return;
	
	if(heap_head == n && heap_tail == n){
		heap_head = NULL;
		heap_tail = NULL;
	}
	else if(heap_head == n){
		heap_head = n->next;
		n->next->prev = NULL;
	}
	else if(heap_tail == n){
		heap_tail = n->prev;
		n->prev->next = NULL;
	}
	else{
		n->next->prev = n->prev;
		n->prev->next = n->next;
	}
}

void node_coalesce_with_next(Header* n) {
	
	if(n->next == NULL)
		return;
	
	Header* next_node = n->next;
	n->size += next_node->size + sizeof(Header);
	
	if(n->next->next == NULL){
		heap_tail = n;
		n->next = NULL;
	}
	else{
		next_node->next->prev = n;
		n->next = next_node->next;
	}
}

Header* node_coalesce_with_neighbors(Header* n) {
	
	if(n->prev == NULL && n->next == NULL)
		return n;
	
	Header* prev = n->prev;
	
	if(prev == NULL){
		if(n->next->used == false)
			node_coalesce_with_next(n);
		return n;
	}
	
	if(n->next == NULL){
		if(prev->used == false){
			node_coalesce_with_next(prev);
			return prev;
		}
		return n;
	}
	
	if(n->next->used == false && prev->used == false){
		node_coalesce_with_next(prev);
		node_coalesce_with_next(prev);
		return prev;
	}
	else if(n->next->used == false){
		node_coalesce_with_next(n);
		return n;
	}
	else if(prev->used == false){
		node_coalesce_with_next(prev);
		return prev;
	}
	else
		return n;
	
}

Header* findFreeBlock(int size){
	
	for(Header* n = heap_head; n != NULL; n = n->next)
		if(n->used == false)
			if(n->size >= size){
				n->used = true;
				if(n->size >= size + sizeof(Header) + MINIMUM_ALLOCATION){
					Header* new_block = PTR_ADD_BYTES(n, sizeof(Header) + size);
					new_block->used = false;
					new_block->size = n->size - size - sizeof(Header);
					node_link(new_block, n);
					n->size = size;
				}
				
				return n;
			}
	
	return NULL;
}

Header* expand_heap(int size){
	
	Header* header = sbrk(sizeof(Header) + size);
	header->used = true;
	header->size = size;
	node_link(header, heap_tail);
	
	return header;
}

void shrink_heap(){
	
	sbrk(-(sizeof(Header) + heap_tail->size));
	node_unlink(heap_tail);
	
}

// ------------------------------------------------------------------------------------------------
// Public API

void* my_malloc(size_t size) {
	if(size == 0)
		return NULL;

	size = round_up_size(size);
	
	Header* header = findFreeBlock(size);
	if(header == NULL)
		header = expand_heap(size);

	return header + 1;
}

void my_free(void* ptr) {
	if(ptr == NULL)
		return;
	
	Header* header = (Header*) ptr - 1;
	
	header->used = false;
	
	header = node_coalesce_with_neighbors(header);
	
	if(header == heap_tail)
		shrink_heap();
}

void my_dump(const char* message) {
	printf(("    %s\n"), message);

	if(heap_head == NULL) {
		puts(BLUE("        <empty>"));
	} else {
		printf("        ");

		for(Header* block = heap_head; block != NULL; block = block->next) {
			if(block->used) {
				printf(RED( "[U %ld]"), block->size);
			} else {
				printf(BLUE("[F %ld]"), block->size);
			}
		}

		printf("\n");
	}
}