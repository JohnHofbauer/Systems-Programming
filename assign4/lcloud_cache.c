////////////////////////////////////////////////////////////////////////////////
//
//  File           : lcloud_cache.c
//  Description    : This is the cache implementation for the LionCloud
//                   assignment for CMPSC311.
//
//   Author        : Patrick McDaniel
//   Last Modified : Thu 19 Mar 2020 09:27:55 AM EDT
//

// Includes 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cmpsc311_log.h>
#include <lcloud_cache.h>

// Information
//
// 64 blocks of cache, defined as LC_CACHE_MAXBLOCKS
// 

struct Cache{
    // Placement of where the block is from.
    int Device;
    int Sector;
    int Block;

    // The time at which the data was put in
    int Time;

    // array for all the blocks of cache
    char cache[255]; // there is 256 charicters in each block of cache

}* LcCachePtr; // There are 64 blocks of cache

// To hold the number of cach acceses.
int Number_Of_Accesses = 1;

//
// Functions
//


////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcloud_getcache
// Description  : Search the cache for a block 
//
// Inputs       : did - device number of block to find
//                sec - sector number of block to find
//                blk - block number of block to find
// Outputs      : cache block if found (pointer), NULL if not or failure
char * lcloud_getcache( LcDeviceId did, uint16_t sec, uint16_t blk ) {
    logMessage(LOG_OUTPUT_LEVEL, "          ### Checking cache for data.");

    // Find the block within the cache.
    for(int index = 0; index < LC_CACHE_MAXBLOCKS; index++) {
        

        // LcCachePtr[index].Time != 0  // make sure the block is inisilized before checking values that may not exsist
        if (LcCachePtr[index].Time != 0 && LcCachePtr[index].Device == did && LcCachePtr[index].Sector == sec && LcCachePtr[index].Block == blk) 
        {
            // Update timmer
            LcCachePtr[index].Time = Number_Of_Accesses;
            // Increase the access ammount.
            Number_Of_Accesses++;
            
            if (LcCachePtr[index].cache == NULL) {
                return(NULL);
            }
            return(LcCachePtr[index].cache);
        }
    } 
    /* Return not found */
    return(NULL);  
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcloud_putcache
// Description  : Put a value in the cache 
//
// Inputs       : did - device number of block to insert
//                sec - sector number of block to insert
//                blk - block number of block to insert
// Outputs      : 0 if succesfully inserted, -1 if failure
int lcloud_putcache( LcDeviceId did, uint16_t sec, uint16_t blk, char * block ) {
    logMessage(LOG_OUTPUT_LEVEL, "          ### Writting Data to Cache.");

    int Lowest_Time = LcCachePtr[0].Time;
    int Block_Placement = (int)0;

    // Find the lowest time to put the data.
    for(int index = 1; index < LC_CACHE_MAXBLOCKS; index++) {
        if (LcCachePtr[index].Time < Lowest_Time) {
            
            // Chose the oldest/empty block
            Lowest_Time = LcCachePtr[index].Time;
            Block_Placement = index;
        } 
    }   

    //// CHECK IF A PREVOUS VERSION OF THE DATA ALLREADY EXISITS
    // Find the block within the cache.
    for(int index = 0; index < LC_CACHE_MAXBLOCKS; index++) {
        // LcCachePtr[index].Time != 0  // make sure the block is inisilized before checking values that may not exsist
        if (LcCachePtr[index].Time != 0 && LcCachePtr[index].Device == did && LcCachePtr[index].Sector == sec && LcCachePtr[index].Block == blk) 
        {
            Lowest_Time = LcCachePtr[index].Time;
            Block_Placement = index;
        }
    }

    // Save the placement for the block
    LcCachePtr[Block_Placement].Device = did;
    LcCachePtr[Block_Placement].Sector = sec;
    LcCachePtr[Block_Placement].Block = blk;
    LcCachePtr[Block_Placement].Time = Number_Of_Accesses;

    // Increase the access ammount.
    Number_Of_Accesses++;

    // Copy the block
    memcpy(LcCachePtr[Block_Placement].cache, block, 256);

    /* Return successfully */
    return( 0 );
    //}

}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcloud_initcache
// Description  : Initialze the cache by setting up metadata a cache elements.
//
// Inputs       : maxblocks - the max number number of blocks 
// Outputs      : 0 if successful, -1 if failure
int lcloud_initcache( int maxblocks ) {

    LcCachePtr = (struct Cache *) malloc(maxblocks * sizeof(struct Cache));

    // INILIZE THE TIME: 
    for(int index = 0; index < maxblocks; index++) {
        LcCachePtr[index].Time = 0;
    }

    /* Return successfully */
    return( 0 );
}
////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcloud_closecache
// Description  : Clean up the cache when program is closing
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int lcloud_closecache( void ) {

    // Return the data back to the void.
    free(LcCachePtr);

    logMessage(LOG_OUTPUT_LEVEL, "          ### The cache is free.");

    /* Return successfully */
    return( 0 );
}