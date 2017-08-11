/*
 * CS 2110 Fall 2016
 * Author: KASHYAP PATEL
 */

/* we need this for uintptr_t */
#include <stdint.h>
/* we need this for memcpy/memset */
#include <string.h>
/* we need this for my_sbrk */
#include "my_sbrk.h"
/* we need this for the metadata_t struct definition */
#include "my_malloc.h"

/* You *MUST* use this macro when calling my_sbrk to allocate the
 * appropriate size. Failure to do so may result in an incorrect
 * grading!
 */
#define SBRK_SIZE 2048

/* If you want to use debugging printouts, it is HIGHLY recommended
 * to use this macro or something similar. If you produce output from
 * your code then you may receive a 20 point deduction. You have been
 * warned.
 */
#ifdef DEBUG
#define DEBUG_PRINT(x) printf(x)
#else
#define DEBUG_PRINT(x)
#endif

/* Our freelist structure - this is where the current freelist of
 * blocks will be maintained. failure to maintain the list inside
 * of this structure will result in no credit, as the grader will
 * expect it to be maintained here.
 * DO NOT CHANGE the way this structure is declared
 * or it will break the autograder.
 */
metadata_t* freelist;


/*
 * iterate through the freelist
 * find and return the smallest block that is big enough to handle the inputted size
 */
void* find_smallest_fit(size_t size) {

	metadata_t* current  = freelist;

	//keep going until the current block is big enough to fulfill the size you want
	while (current != NULL && current->block_size < size) {

		current = current->next;
	}

	//return the pointer of block once found
	return current;

}

/*
 * method for whether there's enough space for a block to split into two
 * if yes return 1, if no return 0
 */
int should_split(metadata_t* block, size_t size) {

	//this is the extra size on top of the block you need
	size_t buffer_size = sizeof(metadata_t) + sizeof(int);

	//if there's extra size left for the buffer size, you're good
	if ((block->block_size - size) > buffer_size) {
		return 1;
	}

	return 0;

}

/*
 * method for merging a block with its next block in the freelist
 * doesn't return anything, only freelist variable is updated
 */
void merge(metadata_t* block) {

	//add next block's size onto previous block's size
	block->block_size += block->next->block_size;

	//skip the "next" block since you updated the prev block's size to be equal to both blocks
	block->next = block->next->next;

}

/*
 * add a designated block to the freelist
 * doesn't return anything, only freelist variable is updated
 */
void add_to_freelist(metadata_t* block) {

	//if there's nothing in the freelist (AKA null), just assign it to the block passed in
	if (freelist == NULL) {
		freelist = block;
	} else {

		//current and previous variables while iterating in freelist
		metadata_t* current = freelist;
		metadata_t* prev = NULL;

		//loop until the address of current is less than address of block since we are ordering freelist that way
		while (current != NULL && ((uintptr_t) block > (uintptr_t) current)) {
				//update prev and current accordingly
				prev = current;
				current = current->next;
		}

		//after you're out of loop, you add the block between prev and current
		block->next = current;

		//checks to see if the address pointer after the block is the same as the current pointer
		//aka if the block and current are in the same node, then merge them
		if ((metadata_t*) ((char*) block + block->block_size) == current) {
			merge(block);
		}

		//if prev was never initialized, that means you're at the front of the list or its size 1
		if (prev == NULL) {
			//just make the head of the list (freelist var = head) equal to the block
			freelist = block;
		} else {

			//if prev was initialized, it's next pointer points to the block
			prev->next = block;
			//check to see if the block's pointer is DIRECTLY after the prev block
			//if it is, then merge the two since they're in the same node
			if ((metadata_t*) ((char*) prev + prev->block_size) == block) {
				merge(prev);
			}
		}
	}

}

/*
 * remove the inputted block from the freelist
 * return the block that you removed
 */
void* remove_from_freelist(metadata_t* block) {

	metadata_t* current = freelist;
	metadata_t* prev = NULL;

	//iterate while the block pointer isn't the same as the current pointer
	while (current != NULL && block != current) {
		prev = current;
		current = current->next;
	}

	//if prev variable was initialized, you basically skip over the block so assign prev's next pointer accordingly
	if (prev != NULL) {
		prev->next = current->next;
	} else {
		//if prev's variable wasn't initialized (AKA at front of list)
		//then just point the head variable to the node after the first
		freelist = current->next;
	}

	//return the node you removed from the freelist
	return current;

}

/*
 * split a block in two
 * return the pointer to the first part of the split block 
 */
void* split_block(metadata_t* block, size_t size) {

	//first remove the entire block from the freelist
	metadata_t* removed = remove_from_freelist(block);

	//actually split the block: make a pointer to a second block that's in the middle of the original block
	metadata_t* second_block = (metadata_t*) ((char*) block + size);

	//the size of the second block is the original block's size minus the size of the first block
	second_block->block_size = block->block_size - size;

	//add the second (newly created) block back to the freelist
	add_to_freelist(second_block);

	//return the block that's updated to only contain the first part of the original block
	return removed;
}

/*
 * calculates a block's canary
 */
int calc_canary(metadata_t* block) {
	return ((int) (block->block_size << 16) | block->request_size) ^ (int) (uintptr_t) block;
}


void* my_malloc(size_t size) {

	//calculate the actual size that you need for the block
	size_t actual_size = size + sizeof(metadata_t) + sizeof(int);

	//if the actual size is greater than 2048, handle error
	if (actual_size > SBRK_SIZE) {
		ERRNO = SINGLE_REQUEST_TOO_LARGE;
		return NULL;
	}

	//go through the freelist and find a block with a big enough size
	metadata_t* block = find_smallest_fit(actual_size);

	//if there's no such block in the freelist, we make our own (AKA freelist is empty)
	if (block == NULL) {

		//call sbrk to allocate a block
		block = my_sbrk(SBRK_SIZE);

		//if sbrk happens to return NULL, handle error accordingly
		if (block == NULL) {
			ERRNO = OUT_OF_MEMORY;
			return NULL;
		}

		//set the attributes of the block accordingly
		block->block_size = SBRK_SIZE;
		block->request_size = 0;
		block->next = NULL;

		//now we add the block to the freelist (freelist no longer empty)
		add_to_freelist(block);
		//now that the freelist is populated with the block, call malloc again and other cases will be called
		return my_malloc(size);

	//if the block in the freelist has enough space to be split into two
	} else if (should_split(block, actual_size)) {

		//split the block, what's returned is the first part of the split block
		block = split_block(block, actual_size);

		//set the attributes accordingly (basically update the block size to only be the first part of the split block)
		block->block_size = actual_size;
		block->request_size = size;

		//calculate the canary in the metadata (front part of the block)
		block->canary = calc_canary(block);
		//calculate the canary in the back of the block
		*((int*) ((char*) block + sizeof(metadata_t) + size)) = calc_canary(block);

		//no error occured, return address of block 
		ERRNO = NO_ERROR;
		return block + 1;

	}

	//if the block should NOT be split, just remove it from the freelist
	block = remove_from_freelist(block);

	//set the attributes accordingly (updating block size)
	block->request_size = size;
	block->block_size = actual_size;

	//calculate canary in the front (in metadata)
	block->canary = calc_canary(block);
	//calculate canary in the back of the block
	*((int*) ((char*) block + sizeof(metadata_t) + size)) = calc_canary(block);

	//no error occured, return address of block
	ERRNO = NO_ERROR;
	return block + 1;

}

void* my_realloc(void* ptr, size_t new_size) {

	//you're not reallocating anything, just doing a normal malloc call
    if (ptr == NULL) {
    	return my_malloc(new_size);
    }

    //if you're not trying to move anything around (AKA reallocating anything), just free the block you passed in
    if (new_size == 0) {
    	my_free(ptr);
    	return NULL;
    }

    //call malloc to get the pointer of the block that you're copying info to
    char* address = my_malloc(new_size);

    //copy over the information from the pointer passed in to the block pointer you retrieved from malloc
    memcpy(address, ptr, new_size);

    //free the pointer passed in - don't need it anymore
    my_free(ptr);

    //return the pointer you got from malloc
    return address;

}

void* my_calloc(size_t nmemb, size_t size) {
    
    //get the pointer that you're zeroing from malloc
    char* address = my_malloc(nmemb * size);

    //zero out everything starting at the address pointer, going til you reach the pointer + nmemb * size
    memset(address, 0, nmemb * size);

    //return the address of the zeroed out data
    return address;

}

void my_free(void* ptr) {

	//this is the actual starting point of the block
	metadata_t* block = (metadata_t*) ((char*) ptr - sizeof(metadata_t));

	//what the canary's supposed to be
	int actual_canary = calc_canary(block);

	//retrieve the canary from the end of the block
	int back_canary = *((int*) ((char*) block + sizeof(metadata_t) + block->request_size));

	//check to see if canaries in the front and back are corrupted
	if (block->canary != actual_canary || actual_canary != back_canary) {
		//if they are, set appropriate error code and return
		ERRNO = CANARY_CORRUPTED;
		return;
	}

	//if canaries aren't corrupted, add the block back to the freelist and you're good
	add_to_freelist(block);
	//set error accordingly
	ERRNO = NO_ERROR;

}