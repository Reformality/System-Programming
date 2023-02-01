#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "myMalloc.h"
#include "printing.h"

/* Due to the way assert() prints error messges we use out own assert function
 * for deteminism when testing assertions
 */
#ifdef TEST_ASSERT
  inline static void assert(int e) {
    if (!e) {
      const char * msg = "Assertion Failed!\n";
      write(2, msg, strlen(msg));
      exit(1);
    }
  }
#else
  #include <assert.h>
#endif

/*
 * Mutex to ensure thread safety for the freelist
 */
static pthread_mutex_t mutex;

/*
 * Array of sentinel nodes for the freelists
 */
header freelistSentinels[N_LISTS];

/*
 * Pointer to the second fencepost in the most recently allocated chunk from
 * the OS. Used for coalescing chunks
 */
header * lastFencePost;

/*
 * Pointer to maintian the base of the heap to allow printing based on the
 * distance from the base of the heap
 */ 
void * base;

/*
 * List of chunks allocated by  the OS for printing boundary tags
 */
header * osChunkList [MAX_OS_CHUNKS];
size_t numOsChunks = 0;

// ----- MY HELPER Global Variables -----
/*
 * Pointer to the second fencepost in the most recently allocated chunk from
 * the OS. Used for coalescing chunks
 */
header * lastestHeadFencePost;
// ----- MY HELPER Global Variables END -----

/*
 * direct the compiler to run the init function before running main
 * this allows initialization of required globals
 */
static void init (void) __attribute__ ((constructor));

// Helper functions for manipulating pointers to headers
static inline header * get_header_from_offset(void * ptr, ptrdiff_t off);
static inline header * get_left_header(header * h);
static inline header * ptr_to_header(void * p);

// Helper functions for allocating more memory from the OS
static inline void initialize_fencepost(header * fp, size_t object_left_size);
static inline void insert_os_chunk(header * hdr);
static inline void insert_fenceposts(void * raw_mem, size_t size);
static header * allocate_chunk(size_t size);

// Helper functions for freeing a block
static inline void deallocate_object(void * p);

// Helper functions for allocating a block
static inline header * allocate_object(size_t raw_size);

// Helper functions for verifying that the data structures are structurally 
// valid
static inline header * detect_cycles();
static inline header * verify_pointers();
static inline bool verify_freelist();
static inline header * verify_chunk(header * chunk);
static inline bool verify_tags();

// ----- MY HELPER FUNCTIONS -----
static inline header * find_block(size_t raw_size);
static inline header * split_block(header * bigBlock, size_t raw_size);
static inline void request_chunk();

static inline header * free_block(header * target);
static inline void insert_freelist(header * node);
static inline void remove_freelist(header * node);
static inline header * get_freelist_sentinel(header * node);



static void init();

static bool isMallocInitialized;

/**
 * @brief Helper function to retrieve a header pointer from a pointer and an 
 *        offset
 *
 * @param ptr base pointer
 * @param off number of bytes from base pointer where header is located
 *
 * @return a pointer to a header offset bytes from pointer
 */
static inline header * get_header_from_offset(void * ptr, ptrdiff_t off) {
	return (header *)((char *) ptr + off);
}

/**
 * @brief Helper function to get the header to the right of a given header
 *
 * @param h original header
 *
 * @return header to the right of h
 */
header * get_right_header(header * h) {
	return get_header_from_offset(h, get_object_size(h));
}

/**
 * @brief Helper function to get the header to the left of a given header
 *
 * @param h original header
 *
 * @return header to the right of h
 */
inline static header * get_left_header(header * h) {
  return get_header_from_offset(h, -h->object_left_size);
}

/**
 * @brief Fenceposts are marked as always allocated and may need to have
 * a left object size to ensure coalescing happens properly
 *
 * @param fp a pointer to the header being used as a fencepost
 * @param object_left_size the size of the object to the left of the fencepost
 */
inline static void initialize_fencepost(header * fp, size_t object_left_size) {
	set_object_state(fp,FENCEPOST);
	set_object_size(fp, ALLOC_HEADER_SIZE);
	fp->object_left_size = object_left_size;
}

/**
 * @brief Helper function to maintain list of chunks from the OS for debugging
 *
 * @param hdr the first fencepost in the chunk allocated by the OS
 */
inline static void insert_os_chunk(header * hdr) {
  if (numOsChunks < MAX_OS_CHUNKS) {
    osChunkList[numOsChunks++] = hdr;
  }
}

/**
 * @brief given a chunk of memory insert fenceposts at the left and 
 * right boundaries of the block to prevent coalescing outside of the
 * block
 *
 * @param raw_mem a void pointer to the memory chunk to initialize
 * @param size the size of the allocated chunk
 */
inline static void insert_fenceposts(void * raw_mem, size_t size) {
  // Convert to char * before performing operations
  char * mem = (char *) raw_mem;

  // Insert a fencepost at the left edge of the block
  header * leftFencePost = (header *) mem;
  initialize_fencepost(leftFencePost, ALLOC_HEADER_SIZE);

  // Insert a fencepost at the right edge of the block
  header * rightFencePost = get_header_from_offset(mem, size - ALLOC_HEADER_SIZE);
  initialize_fencepost(rightFencePost, size - 2 * ALLOC_HEADER_SIZE);
}

/**
 * @brief Allocate another chunk from the OS and prepare to insert it
 * into the free list
 *
 * @param size The size to allocate from the OS
 *
 * @return A pointer to the allocable block in the chunk (just after the 
 * first fencpost)
 */
static header * allocate_chunk(size_t size) {
  void * mem = sbrk(size);
  
  insert_fenceposts(mem, size);
  header * hdr = (header *) ((char *)mem + ALLOC_HEADER_SIZE);
  set_object_state(hdr, UNALLOCATED);
  set_object_size(hdr, size - 2 * ALLOC_HEADER_SIZE);
  hdr->object_left_size = ALLOC_HEADER_SIZE;
  return hdr;
}

/*-------------------------------- MY HELPER FUNCTION FOR ALLOCATE --------------------------------*/

static inline header * find_block(size_t raw_size) { 
  header * returnBlock = NULL;  //return this pointer
  int flag = 0;
  
  while (returnBlock == NULL) {
    //freelist from 0 to N_LISTS - 2
    flag = 0;
    int i = 0;  //index for freelist search
    for (i = 0; i < N_LISTS - 1; i++) {
      header * freelist = &freelistSentinels[i];
      if (raw_size <= (i+1) * sizeof(header *)) {
        if (freelist->next != freelist) {
          returnBlock = freelist->next;
          remove_freelist(returnBlock);
          break;
        }
      }
    }
    //freelist N_LISTS - 1
    if (i == N_LISTS - 1) {
      flag = 1;
      //find the block
      header * freelist = &freelistSentinels[N_LISTS - 1];
      header * freeNode = freelist->next;
      while (freeNode != freelist) {
        if (get_object_size(freeNode) >= raw_size + ALLOC_HEADER_SIZE) {
          returnBlock = freeNode;
          break;
        }
        freeNode = freeNode->next;
      }
    }
    //if returnBlock is still null, request another chunk
    if (returnBlock == NULL) {
      request_chunk();
    }
  }

  //decide if to split
  size_t sizeDiff = get_object_size(returnBlock) - raw_size - ALLOC_HEADER_SIZE;
  if (sizeDiff >= sizeof(header)) { //split block
    returnBlock = split_block(returnBlock, raw_size);
  } else {  //don't split, set states
    set_object_state(returnBlock, ALLOCATED);
    if (flag) { //last freelist and don't split
      remove_freelist(returnBlock);
    }
  }

  return returnBlock;
}

static inline header * split_block(header * bigBlock, size_t raw_size) {
  header * returnBlock = NULL;
  size_t bigBlockSize = get_object_size(bigBlock);  //old big block size

  returnBlock = (header *) ((char *)bigBlock + get_object_size(bigBlock) - raw_size - ALLOC_HEADER_SIZE);  //init the returnblock
  set_object_size(bigBlock, get_object_size(bigBlock) - raw_size - ALLOC_HEADER_SIZE);  //update the bigBlock size
  set_object_state(returnBlock, ALLOCATED); //update the returnblock state
  set_object_size(returnBlock, raw_size + ALLOC_HEADER_SIZE); //update the returnblock size
  returnBlock->object_left_size = get_object_size(bigBlock);  //set the left size for returnblock
  header * rightBlock = get_right_header(returnBlock);  //find the right block
  rightBlock->object_left_size = get_object_size(returnBlock);  //set right block's left-block-size

  if (bigBlockSize > (N_LISTS-1) * sizeof(header *)) { //big block in the last free list
    if (get_object_size(bigBlock) <= (N_LISTS-1) * sizeof(header *)) {  //change big block freelist position if needed
      // remove_freelist(bigBlock);
      bigBlock->next->prev = bigBlock->prev;
      bigBlock->prev->next = bigBlock->next;
      insert_freelist(bigBlock);
    }
  } else {  //big block not in the last free list
    insert_freelist(bigBlock);
  }
  return returnBlock;
}

static inline void request_chunk() {
  // Allocate a new chunk from the OS
  header * block = allocate_chunk(ARENA_SIZE);
  header * headFencePost = get_header_from_offset(block, -ALLOC_HEADER_SIZE);
  header * checkLastChunkFencePost = get_header_from_offset(block, -ALLOC_HEADER_SIZE * 2);
  if (checkLastChunkFencePost == lastFencePost) { //coalescing chunks 
    header * newFreeBlock = checkLastChunkFencePost;  //starting a new pointer for new block
    size_t tempLeftSize = checkLastChunkFencePost->object_left_size;  //get the left block size
    set_object_size(newFreeBlock, get_object_size(block) + 2 * ALLOC_HEADER_SIZE);  //set new block size
    set_object_state(newFreeBlock, UNALLOCATED);  //set new block state
    newFreeBlock->object_left_size = tempLeftSize;  //set new block left size
    header * rightBlock = get_right_header(newFreeBlock);  //find the right block
    rightBlock->object_left_size = get_object_size(newFreeBlock);
    header * leftBlock = get_left_header(newFreeBlock); //find the left block
    size_t tempSizeLeftBlock = get_object_size(leftBlock);
    if (get_object_state(leftBlock) == UNALLOCATED) { //coalescing with previous free block
      // header * leftFreelistNode = leftBlock->prev;   //get the prev and next in the freelist
      // header * rightFreelistNode = leftBlock->next;
      set_object_size(leftBlock, get_object_size(newFreeBlock) + get_object_size(leftBlock)); //set the target size
      rightBlock->object_left_size = get_object_size(leftBlock);
      
      if (tempSizeLeftBlock <= (sizeof(header *) * (N_LISTS - 1)) + ALLOC_HEADER_SIZE) {  //node not the last freelist sentinel, then change location in freelist
        remove_freelist(leftBlock);
        insert_freelist(leftBlock);
      }
    } else {  //not coalescing with previous block
      insert_freelist(newFreeBlock);
    }
    lastFencePost = get_header_from_offset(newFreeBlock, get_object_size(newFreeBlock));

  } else {  //not coalescing chunks
    insert_os_chunk(headFencePost);
    insert_freelist(block);
    lastestHeadFencePost = headFencePost;
    lastFencePost = get_header_from_offset(block, get_object_size(block));
  }
}

/*------------------------------ MY HELPER FUNCTION FOR ALLOCATE END ------------------------------*/

/**
 * @brief Helper allocate an object given a raw request size from the user
 *
 * @param raw_size number of bytes the user needs
 *
 * @return A block satisfying the user's request
 */
static inline header * allocate_object(size_t raw_size) {
  if (raw_size == 0) {
    return NULL;
  }
  raw_size = ((raw_size+7) & ~7); 
  if (raw_size < 16) raw_size = 16; //correct the raw size, starting from 16 bytes
  header * new_block = find_block(raw_size); //get the new block that is big enough for the input
  return (void *)((char *) new_block + ALLOC_HEADER_SIZE);  //return the user to the data field
}

/**
 * @brief Helper to get the header from a pointer allocated with malloc
 *
 * @param p pointer to the data region of the block
 *
 * @return A pointer to the header of the block
 */
static inline header * ptr_to_header(void * p) {
  return (header *)((char *) p - ALLOC_HEADER_SIZE); //sizeof(header));
}

/*-------------------------------- MY HELPER FUNCTION FOR DE-ALLOCATE --------------------------------*/
static inline header * free_block(header * target) {  //TODO
  set_object_state(target, UNALLOCATED); //set the target it to UNALLOCATED
  header * leftBlock = get_left_header(target);
  header * rightBlock = get_right_header(target);
  
  if (get_object_state(leftBlock) != UNALLOCATED && get_object_state(rightBlock) != UNALLOCATED) {  // left and right both are not free
    insert_freelist(target);
  } else if (get_object_state(leftBlock) != UNALLOCATED && get_object_state(rightBlock) == UNALLOCATED) { //only right is free
    header * leftFreelistNode = rightBlock->prev;   //get the prev and next in the freelist
    header * rightFreelistNode = rightBlock->next;
    target->prev = leftFreelistNode;  //reset the prev and next onto the target from right block
    target->next = rightFreelistNode;
    leftFreelistNode->next = target;
    rightFreelistNode->prev = target;
    set_object_size(target, get_object_size(target) + get_object_size(rightBlock)); //set the target size
    header * rightRightBlock = get_right_header(rightBlock);  //get and set the right right block's left size
    rightRightBlock->object_left_size = get_object_size(target);

    if (get_object_size(target) <= (sizeof(header *) * (N_LISTS - 1)) + ALLOC_HEADER_SIZE) {  //node not the last freelist sentinel, then change location in freelist
      remove_freelist(target);
      insert_freelist(target);
    } 
  } else if (get_object_state(leftBlock) == UNALLOCATED && get_object_state(rightBlock) != UNALLOCATED) { //only left is free
    // header * leftFreelistNode = leftBlock->prev;   //get the prev and next in the freelist
    // header * rightFreelistNode = leftBlock->next;
    set_object_size(leftBlock, get_object_size(target) + get_object_size(leftBlock)); //set the target size
    rightBlock->object_left_size = get_object_size(leftBlock);
    
    if (get_object_size(leftBlock) <= (sizeof(header *) * (N_LISTS - 1)) + ALLOC_HEADER_SIZE) {  //node not the last freelist sentinel, then change location in freelist
      remove_freelist(leftBlock);
      insert_freelist(leftBlock);
    }
    target = leftBlock; 
  } else {  //----- TODO: case when left and right are free -----
    set_object_size(leftBlock, get_object_size(leftBlock) + get_object_size(target) + get_object_size(rightBlock));
    header * rightRightBlock = get_right_header(rightBlock);  //get and set the right right block's left size
    rightRightBlock->object_left_size = get_object_size(leftBlock);
    remove_freelist(rightBlock);

    if (get_object_size(leftBlock) <= (sizeof(header *) * (N_LISTS - 1)) + ALLOC_HEADER_SIZE) {  //node not the last freelist sentinel, then change location in freelist
      remove_freelist(leftBlock);
      insert_freelist(leftBlock);
    }
    target = leftBlock; 
  }

  //testing
  return target;
}

static inline header * get_freelist_sentinel(header * node) {   //get the freelist sentinel from a node in freelist
  header * freelist;
  int alloSize = get_object_size(node) - ALLOC_HEADER_SIZE; //get the allocated size of this node
  if (alloSize <= (sizeof(header *) * (N_LISTS - 1))) { //node not the last freelist sentinel
    freelist = &freelistSentinels[alloSize / sizeof(header*) - 1];
  } else {  //last sentinel
    freelist = &freelistSentinels[N_LISTS - 1];
  }

  return freelist;
}

static inline void insert_freelist(header * node) {   //insert the node to the "front" of this freelist linkedlist
  header * freelist = get_freelist_sentinel(node);  //get the sentinel
  header * nextNode = freelist->next;
  freelist->next = node;
  node->prev = freelist;
  node->next = nextNode;
  nextNode->prev = node;
}

static inline void remove_freelist(header * node) {
  //remove located block        
  header * prevNode = node->prev;
  header * nextNode = node->next;
  header * freelist = get_freelist_sentinel(node);
  //check if it's the last node
  if (node->next == freelist) {
    prevNode->next = freelist;
    freelist->prev = prevNode;
  } else { //not the last node
    prevNode->next = nextNode;
    nextNode->prev = prevNode;
  }
}

/*------------------------------ MY HELPER FUNCTION FOR DE-ALLOCATE END ------------------------------*/

/**
 * @brief Helper to manage deallocation of a pointer returned by the user
 *
 * @param p The pointer returned to the user by a call to malloc
 */
static inline void deallocate_object(void * p) {  // TODO implement deallocation
  if (p == NULL) {  //free null case
    return;
  }
  header * freeBlock = ptr_to_header(p);
  if (get_object_state(freeBlock) == UNALLOCATED) { //prevent double free
    printf("Double Free Detected\n");
    printf("Assertion Failed!\n");
    exit(1);
    return;
  }
  free_block(freeBlock);
}

/**
 * @brief Helper to detect cycles in the free list
 * https://en.wikipedia.org/wiki/Cycle_detection#Floyd's_Tortoise_and_Hare
 *
 * @return One of the nodes in the cycle or NULL if no cycle is present
 */
static inline header * detect_cycles() {
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    for (header * slow = freelist->next, * fast = freelist->next->next; 
         fast != freelist; 
         slow = slow->next, fast = fast->next->next) {
      if (slow == fast) {
        return slow;
      }
    }
  }
  return NULL;
}

/**
 * @brief Helper to verify that there are no unlinked previous or next pointers
 *        in the free list
 *
 * @return A node whose previous and next pointers are incorrect or NULL if no
 *         such node exists
 */
static inline header * verify_pointers() {
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    for (header * cur = freelist->next; cur != freelist; cur = cur->next) {
      if (cur->next->prev != cur || cur->prev->next != cur) {
        return cur;
      }
    }
  }
  return NULL;
}

/**
 * @brief Verify the structure of the free list is correct by checkin for 
 *        cycles and misdirected pointers
 *
 * @return true if the list is valid
 */
static inline bool verify_freelist() {
  header * cycle = detect_cycles();
  if (cycle != NULL) {
    fprintf(stderr, "Cycle Detected\n");
    print_sublist(print_object, cycle->next, cycle);
    return false;
  }

  header * invalid = verify_pointers();
  if (invalid != NULL) {
    fprintf(stderr, "Invalid pointers\n");
    print_object(invalid);
    return false;
  }

  return true;
}

/**
 * @brief Helper to verify that the sizes in a chunk from the OS are correct
 *        and that allocated node's canary values are correct
 *
 * @param chunk AREA_SIZE chunk allocated from the OS
 *
 * @return a pointer to an invalid header or NULL if all header's are valid
 */
static inline header * verify_chunk(header * chunk) {
	if (get_object_state(chunk) != FENCEPOST) {
		fprintf(stderr, "Invalid fencepost\n");
		print_object(chunk);
		return chunk;
	}
	
	for (; get_object_state(chunk) != FENCEPOST; chunk = get_right_header(chunk)) {
		if (get_object_size(chunk)  != get_right_header(chunk)->object_left_size) {
			fprintf(stderr, "Invalid sizes\n");
			print_object(chunk);
			return chunk;
		}
	}
	
	return NULL;
}

/**
 * @brief For each chunk allocated by the OS verify that the boundary tags
 *        are consistent
 *
 * @return true if the boundary tags are valid
 */
static inline bool verify_tags() {
  for (size_t i = 0; i < numOsChunks; i++) {
    header * invalid = verify_chunk(osChunkList[i]);
    if (invalid != NULL) {
      return invalid;
    }
  }

  return NULL;
}

/**
 * @brief Initialize mutex lock and prepare an initial chunk of memory for allocation
 */
static void init() {
  // Initialize mutex for thread safety
  pthread_mutex_init(&mutex, NULL);

#ifdef DEBUG
  // Manually set printf buffer so it won't call malloc when debugging the allocator
  setvbuf(stdout, NULL, _IONBF, 0);
#endif // DEBUG

  // Allocate the first chunk from the OS
  header * block = allocate_chunk(ARENA_SIZE);

  header * prevFencePost = get_header_from_offset(block, -ALLOC_HEADER_SIZE);
  insert_os_chunk(prevFencePost);

  lastFencePost = get_header_from_offset(block, get_object_size(block));

  // Set the base pointer to the beginning of the first fencepost in the first
  // chunk from the OS
  base = ((char *) block) - ALLOC_HEADER_SIZE; //sizeof(header);
  lastestHeadFencePost = base;

  // Initialize freelist sentinels
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    freelist->next = freelist;
    freelist->prev = freelist;
  }

  // Insert first chunk into the free list
  header * freelist = &freelistSentinels[N_LISTS - 1];
  freelist->next = block;
  freelist->prev = block;
  block->next = freelist;
  block->prev = freelist;
}

/* 
 * External interface
 */
void * my_malloc(size_t size) {
  pthread_mutex_lock(&mutex);
  header * hdr = allocate_object(size); 
  pthread_mutex_unlock(&mutex);
  return hdr;
}

void * my_calloc(size_t nmemb, size_t size) {
  return memset(my_malloc(size * nmemb), 0, size * nmemb);
}

void * my_realloc(void * ptr, size_t size) {
  void * mem = my_malloc(size);
  memcpy(mem, ptr, size);
  my_free(ptr);
  return mem; 
}

void my_free(void * p) {
  pthread_mutex_lock(&mutex);
  deallocate_object(p);
  pthread_mutex_unlock(&mutex);
}

bool verify() {
  return verify_freelist() && verify_tags();
}
