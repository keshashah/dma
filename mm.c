/*@author :- KESHA SHAH @DAIICT ID :- 201101228*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    /* Team name */
    "A-Team",
    /* First member's full name */
    "KESHA SHAH",
    /* First member's email address */
    "201101228@daiict.ac.in",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/*Additional Macros defined*/
#define WSIZE 4                                                                             
#define DSIZE 8                                                                             
#define CHUNKSIZE 16                                                                        
#define OVERHEAD 24                                                                         
#define MAX(x ,y)  ((x) > (y) ? (x) : (y))                                                  
#define PACK(size, alloc)  ((size) | (alloc))                                               
#define GET(p)  (*(size_t *)(p))                                                            
#define PUT(p, value)  (*(size_t *)(p) = (value))                                           
#define GET_SIZE(p)  (GET(p) & ~0x7)                                                        
#define GET_ALLOC(p)  (GET(p) & 0x1)                                                        
#define HDRP(bp)  ((void *)(bp) - WSIZE)                                                    
#define FTRP(bp)  ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)                               
#define NEXT_BLKP(bp)  ((void *)(bp) + GET_SIZE(HDRP(bp)))                                  
#define PREV_BLKP(bp)  ((void *)(bp) - GET_SIZE(HDRP(bp) - WSIZE))                          
#define NEXT_FREEP(bp)  (*(void **)(bp + WSIZE))                                            
#define PREV_FREEP(bp)  (*(void **)(bp))                                                    

static char *heap_listp = 0;                                                         
static char *head = 0;                                                               

static void *extend_heap(size_t words);
static void place(void *bp, size_t size);
static void *find_fit(size_t size);
static void *coalesce(void *bp);
static void mm_insert(void *bp);
static void mm_remove(void *bp);


//Initialize the heap in this block by making the prologue epilogue and providing extra padding.Also the prologue has space for prev/next and an empty padding since minimum block size is 24 which is max(min all block size, min free block size)
int mm_init(void)
{
    if((heap_listp = mem_sbrk(2 * OVERHEAD)) == NULL){                                     
        return -1;
    }

    PUT(heap_listp, 0);                                         
    PUT(heap_listp + WSIZE, PACK(OVERHEAD, 1));                 
    PUT(heap_listp + DSIZE, 0);                                 
    PUT(heap_listp + DSIZE + WSIZE, 0);                         
    PUT(heap_listp + OVERHEAD, PACK(OVERHEAD, 1));              
    PUT(heap_listp + WSIZE + OVERHEAD, PACK(0, 1));             
    head = heap_listp + DSIZE;                                  

    if(extend_heap(CHUNKSIZE / WSIZE) == NULL){                 
        return -1;
    }

    return 0;
}
//Here the bp is given to the user if he requests for space in heap.Also here the size allignment issue is taken care of by considering min block size and padding issues
void *mm_malloc(size_t size)
{
    size_t asize;                                               
    size_t extendedsize;                                        
    char *bp;                                                   

    if(size <= 0){                                              
        return NULL;
    }

    asize = MAX(ALIGN(size) + DSIZE, OVERHEAD);                 

    if((bp = find_fit(asize))){                                 
        place(bp, asize);                                       
        return bp;
    }

    extendedsize = MAX(asize, CHUNKSIZE);                       

    if((bp = extend_heap(extendedsize / WSIZE)) == NULL){       
        return NULL;                                            
    }

    place(bp, asize);                                           
    return bp;
}

//This thing frees the block and passes the control to coalesce where add/del of free list is done
void mm_free(void *bp)
{
    if(bp == NULL){                                             
        return;                                                 
    }

    size_t size = GET_SIZE(HDRP(bp));                           

    PUT(HDRP(bp), PACK(size, 0));                               
    PUT(FTRP(bp), PACK(size, 0));                               
    coalesce(bp);                                               
}

void *mm_realloc(void *bp, size_t size)
{
    size_t oldsize;                                                                         //Holds the old size of the block
    void *newbp;                                                                            //Holds the new block pointer
    size_t asize = MAX(ALIGN(size) + DSIZE, OVERHEAD);                               //Calciulate the adjusted size

    if(size <= 0){                                                                          //If size is less than 0
        mm_free(bp);                                                                        //Free the block
        return 0;
    }

    if(bp == NULL){                                                                         //If old block pointer is null, then it is malloc
        return mm_malloc(size);
    }

    oldsize = GET_SIZE(HDRP(bp));                                                           //Get the size of the old block

    if(oldsize == asize){                                                            //If the size of the old block and requested size are same then return the old block pointer
        return bp;
    }

    if(asize <= oldsize){                                                            //If the size needs to be decreased
        size = asize;                                                                //Shrink the block

        if(oldsize - size <= OVERHEAD){                                                     //If the new block cannot be formed
            return bp;                                                                      //Return the old block pointer
        }
                                                                                            //If a new block can be formed
        PUT(HDRP(bp), PACK(size, 1));                                                       //Update the size in the header of the reallocated block
        PUT(FTRP(bp), PACK(size, 1));                                                       //Update the size in the header of the reallocated block
        PUT(HDRP(NEXT_BLKP(bp)), PACK(oldsize - size, 1));                                  //Update the size in the header of the next block
        mm_free(NEXT_BLKP(bp));                                                             //Free the next block
        return bp;
    }
                                                                                            //If the block has to be expanded during reallocation
    newbp = mm_malloc(size);                                                                //Allocate a new block

    if(!newbp){                                                                             //If realloc fails the original block is left as it is
        return 0;
    }

    if(size < oldsize){
        oldsize = size;
    }

    memcpy(newbp, bp, oldsize);                                                             //Copy the old data to the new block
    mm_free(bp);                                                                            //Free the old block
    return newbp;
}

//Extend the heap in case when the heap size is not sufficient to hold the block 
static void* extend_heap(size_t words){
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;                            

    size = MAX(size,OVERHEAD);

    if((int)(bp = mem_sbrk(size)) == -1){                                               
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));         
    PUT(FTRP(bp), PACK(size, 0));         
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); 
  
    return coalesce(bp);                  
}


static void *coalesce(void *bp){
    size_t previous_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;       
    size_t next__alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));                                 
    size_t size = GET_SIZE(HDRP(bp));                                                    

    if(previous_alloc && !next__alloc){                                                  
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));                                           
        mm_remove(NEXT_BLKP(bp));                                                        
        PUT(HDRP(bp), PACK(size, 0));                                                       
        PUT(FTRP(bp), PACK(size, 0));                                                       
    }

    else if(!previous_alloc && next__alloc){                                                
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));                                              
        bp = PREV_BLKP(bp);                                                              
        mm_remove(bp);                                                                   
        PUT(HDRP(bp), PACK(size, 0));                                                      
        PUT(FTRP(bp), PACK(size, 0));                                                      
    }

    else if(!previous_alloc && !next__alloc){                                        
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));       
        mm_remove(PREV_BLKP(bp));                                                    
        mm_remove(NEXT_BLKP(bp));                                                    
        bp = PREV_BLKP(bp);                                                          
        PUT(HDRP(bp), PACK(size, 0));                                                
        PUT(FTRP(bp), PACK(size, 0));                                                
    }
    mm_insert(bp);                                                                   
    return bp;
}

//I inserted the new block at the beginning of the free list
static void mm_insert(void *bp){
    NEXT_FREEP(bp) = head;                                                           
    PREV_FREEP(head) = bp;                                                           
    PREV_FREEP(bp) = NULL;                                                           
    head = bp;                                                                       
}


static void mm_remove(void *bp){
    if(PREV_FREEP(bp) != NULL){                              
        NEXT_FREEP(PREV_FREEP(bp)) = NEXT_FREEP(bp); 
    }else{
        head = NEXT_FREEP(bp);                                                        
    }

    PREV_FREEP(NEXT_FREEP(bp)) = PREV_FREEP(bp);                                      
}


static void *find_fit(size_t size){
    void *bp;

    for(bp = head; GET_ALLOC(HDRP(bp)) == 0; bp = NEXT_FREEP(bp)){                    
        if(size <= GET_SIZE(HDRP(bp))){                                                  
            return bp;                                                                   
        }
    }

    return NULL;                                                                         
}


static void place(void *bp, size_t size){
    size_t totalsize = GET_SIZE(HDRP(bp));                                               

    if((totalsize - size) >= OVERHEAD){                                                  
        PUT(HDRP(bp), PACK(size, 1));                                                    
        PUT(FTRP(bp), PACK(size, 1));                                                    
        mm_remove(bp);                                                                   
        bp = NEXT_BLKP(bp);                                                               
        PUT(HDRP(bp), PACK(totalsize - size, 0));                                         
        PUT(FTRP(bp), PACK(totalsize - size, 0));                                         
        coalesce(bp);                                                                     
    }

    else{                                                                                 
        PUT(HDRP(bp), PACK(totalsize, 1));                                                
        PUT(FTRP(bp), PACK(totalsize, 1));                                                
        mm_remove(bp);                             
    }
}
