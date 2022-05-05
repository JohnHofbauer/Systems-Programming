////////////////////////////////////////////////////////////////////////////////
//
//  File           : lcloud_filesys.c
//  Description    : This is the implementation of the Lion Cloud device 
//                   filesystem interfaces.
//
//   Author        : *** John Hofbauer ***
//   Last Modified : *** 02-12-2020 ***
//

// Include files
#include <stdlib.h>
#include <string.h>
#include <cmpsc311_log.h>
#include <assert.h>

// Project include files
#include <lcloud_cache.h>
#include <lcloud_filesys.h>
#include <lcloud_controller.h>
#include <lcloud_network.h>

//
// File system interface implementation

// Define 
int Cache_Enabled = 1;  //<-- SET TO 1 TO ENABE THE CACHE 

// Create the layout for the 64-bit buss address (the shifting does not work when they are diffrent sizes)
struct Buss{
    uint8_t b0 : 4; // b0: 4-bit intiger
    uint8_t b1 : 4; // b1: 4-bit intiger
    uint8_t c0; // c0: 8-bit intiger
    uint8_t c1; // c1: 8-bit intiger
    uint8_t c2; // c3: 8-bit intiger
    uint64_t d0; // d0: 16-bit intiger
    uint64_t d1; // d1: 16-bit intiger
};

// Create the structure for the location of a block.
struct Block{
    int device;
    int sector;
    int block;
    int allocated;
};

// Create the format/frame for all file handles. (one for every file.)
struct Files{
    char name;
    int length;
    int position;
    int8_t Power;
    int16_t Number; 
    int16_t Device_Id;
    char Path;

    //(hold the data positions.)
    struct Block block[2000]; // <-- May have to have this dynamic.
};

// Creates a File handle for all posible files. 
struct Files FILE_HANDLE[2000]; // <-- May have to have this dynamic.

// Bus is on?
int buss_on = 0;

//int FILE_HANDLE[15];
int Number_Of_Devices_On;

// Create the format/frame for all Devices. 
struct Devices{
    int8_t Power;   // If the device is on
    int16_t Number; // The number for the devce.

    int Number_Of_Sectors;  // Store the number of sectors.
    int Number_Of_Blocks;   // Store the number of blocks; for this current device.

    // 2-D array for the data structure that is used for the divice (what is ued)
    int Used_Blocks [200][200]; // <-- May have to have this dynamic.

    int Device_Full;
}device[15];

// Create a globle verabel, to make sure no two file_handles are given the same number.
int Device_Counter = 0;

// Count the number of files opened
int File_Counter = 0;

struct Cache{

    // Number of hits and misses for finding the data in the cache.
    int hits;
    int misses; 
}Stats;

////////////////////////////////////////////////////////////////////////////////
//
// Function     : create_lcloud_registers
// Description  : Create/bundle the FILE_HANDLE into a 64-bit intiger
//
// Inputs       : a pointer to the struct that holds the 7 feilds (b0, b1, c0, c1, c2, d0, d1) 
// Outputs      : the 64 bit intiger of the packed registers. 
LCloudRegisterFrame create_lcloud_registers(struct Buss *BUSS_ADDRESS){
 
    LCloudRegisterFrame Buss = 0;  // Create a LCloudRegisterFrame verable called Buss, to hold the packed register

    Buss = (Buss << 0) | (BUSS_ADDRESS->b0 & 0xF); // Pack b0 into the first 4 bits of the register. 
    Buss = (Buss << 4) | (BUSS_ADDRESS->b1 & 0xF); // Pack b1 into the seccond 4 bits of the register. 
    Buss = (Buss << 8) | (BUSS_ADDRESS->c0 & 0xFF); // Pack c0 into the first 8 bits of the register.
    Buss = (Buss << 8) | (BUSS_ADDRESS->c1 & 0xFF); // Pack c1 into the seccond 8 bits of the register. 
    Buss = (Buss << 8) | (BUSS_ADDRESS->c2 & 0xFF); // Pack c2 into the third 8 bits of the register. 
    Buss = (Buss << 16) | (BUSS_ADDRESS->d0 & 0xFFFF); // Pack d0 into the first 16 bits of the register. 
    Buss = (Buss << 16) | (BUSS_ADDRESS->d1 & 0xFFFF); // Pack d1 into the second 16 bits of the register. 

    //logMessage(LOG_OUTPUT_LEVEL, "Buss '%li'", Buss); // Log to the debugger the packed register.
    return Buss; // retrun the Buss, veriable. 
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : extract_lcloud_registers
// Description  : Extract/decompose the FILE_HANDLE into it's respecing parts.
//
// Inputs       : A 64 bit intiger that holds the packed register.
// Outputs      : A pointer the the BUSS_ADDRESS struct, that holds all 7 peramiters. 
void extract_lcloud_registers(LCloudRegisterFrame resp, struct Buss *BUSS_ADDRESS) {

    BUSS_ADDRESS->b0 = (resp >> 60) & 0xF; // Extract the first 4 bits from the packed register setting it to the b0 value in the struct.
    BUSS_ADDRESS->b1 = (resp >> 56) & 0xF; // Extract the seccond 4 bits from the packed register setting it to the b1 value in the struct.
    BUSS_ADDRESS->c0 = (resp >> 48) & 0xFF; // Extract the first 8 bits from the packed register setting it to the c0 value in the struct.
    BUSS_ADDRESS->c1 = (resp >> 40) & 0xFF; // Extract the seccond 8 bits from the packed register setting it to the c1 value in the struct.
    BUSS_ADDRESS->c2 = (resp >> 32) & 0xFF; // Extract the third 8 bits from the packed register setting it to the c2 value in the struct.
    BUSS_ADDRESS->d0 = (resp >> 16) & 0xFFFF; // Extract the first 16 bits from the packed register setting it to the d0 value in the struct.
    BUSS_ADDRESS->d1 = (resp) & 0xFFFF; // Extract the secdond 16 bits from the packed register setting it to the d1 value in the struct.
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : Power_On
// Description  : This powers on the buss.
//
// Inputs       : A pointer to the *BUSS_ADDRESS struct
// Outputs      : 0 - if the device has been sucessfully turned on. Else; 1

int Power_On (struct Buss *BUSS_ADDRESS) {

    //1. Pack the registers using your create_lcloud_registers function
    BUSS_ADDRESS->b0 = 0;
    BUSS_ADDRESS->b1 = 0;
    BUSS_ADDRESS->c0 = 0;
    BUSS_ADDRESS->c1 = 0;
    BUSS_ADDRESS->c2 = 0;
    BUSS_ADDRESS->d0 = 0;
    BUSS_ADDRESS->d1 = 0;

    // Pack the register.
    LCloudRegisterFrame Packed_Registers = create_lcloud_registers(BUSS_ADDRESS);

    //2. Set the xfer parameter (NULL for power on)
    void * xfvr = NULL;

    //3. Call the lcloud_io_bus and get the return registers
    Packed_Registers = client_lcloud_bus_request(Packed_Registers, xfvr);

    //4. Unpack the registers using your extract_lcloud_registers function
    extract_lcloud_registers(Packed_Registers, BUSS_ADDRESS);

    //5. Check for the return values in the registers for failures, etc.)
    if (BUSS_ADDRESS->b1 != 1) {
        // Device has failed
        logMessage(LOG_INFO_LEVEL, "Power On FAIL. '%i'", Packed_Registers);
        return (int)1;
    }
    else {
        // Set the globle verabel so that the program knows that the buss is on.
        buss_on = (int)1;

        // Return that the program conpleted sicessufully.
        return (int)0;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : Probe_Buss
// Description  : This wiil pack the 64 bit register with LC_DEVPROBE to probe the buss.
//
// Inputs       : A pointer to the 64bit Bus structure. 
// Outputs      : the values of each of the sections in to bus. most inportently d0, which holds the device_ID.
void Lc_Probe_Buss (struct Buss *BUSS_ADDRESS) {
    // Probe the buss to get the Device ID
    
    // Set the verables for 
    BUSS_ADDRESS->b0 = 0;
    BUSS_ADDRESS->b1 = 0;
    BUSS_ADDRESS->c0 = LC_DEVPROBE;
    BUSS_ADDRESS->c1 = 0;
    BUSS_ADDRESS->c2 = 0;
    BUSS_ADDRESS->d0 = 0;
    BUSS_ADDRESS->d1 = 0;
    
    // Pack the registers.
    LCloudRegisterFrame Packed_Registers = create_lcloud_registers(BUSS_ADDRESS);

    // set the datq/XFVR to NULL, since there is no data being sent.
    void * xfvr = NULL;

    //logMessage(LOG_INFO_LEVEL, "Packed Registers: '%i'", Packed_Registers);

    // Pack the registers.
    Packed_Registers = client_lcloud_bus_request(Packed_Registers, xfvr);
    
    logMessage(LOG_OUTPUT_LEVEL, "          ### BUSS_ADDRESS.d0 = %i ###", Packed_Registers);

    // open: Create a struct, for a file. some verarable in the struct(int) that tells wheate
    extract_lcloud_registers(Packed_Registers, BUSS_ADDRESS);

    logMessage(LOG_OUTPUT_LEVEL, "          ### BUSS_ADDRESS.d0 = %i ###", BUSS_ADDRESS->d0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : Lc_Device_Setup
// Description  : The device ID to see the amount of sectors and blocks that the device has.
//
// Inputs       : A pointer to the 64bit Bus structure and the device ID
// Outputs      : Setup the struct for the current device.
void Lc_Device_Setup (int8_t device_Id) {
    // Log the inputs
    logMessage(LOG_OUTPUT_LEVEL, "          ### Finding the numbers and sectors for device: '%i' ###", device_Id);  

    struct Buss BUSS_ADDRESS;  // Create a object of the structre 
    
    // Set the verables for 
    BUSS_ADDRESS.b0 = 0;
    BUSS_ADDRESS.b1 = 0;
    BUSS_ADDRESS.c0 = LC_DEVINIT;
    BUSS_ADDRESS.c1 = device_Id;
    BUSS_ADDRESS.c2 = 0;
    BUSS_ADDRESS.d0 = 0;
    BUSS_ADDRESS.d1 = 0;
    
    // Pack the registers.
    LCloudRegisterFrame Packed_Registers = create_lcloud_registers(&BUSS_ADDRESS);

    // set the datq/XFVR to NULL, since there is no data being sent.
    void * xfvr = NULL;

    // Pack the registers.
    //Packed_Registers = lcloud_io_bus(Packed_Registers, xfvr);
    Packed_Registers = client_lcloud_bus_request(Packed_Registers, xfvr);
    

    // open: Create a struct, for a file. some verarable in the struct(int) that tells wheate
    extract_lcloud_registers(Packed_Registers, &BUSS_ADDRESS);

    // d0 - holds the number of sectors in the device.
    device[device_Id].Number_Of_Sectors = BUSS_ADDRESS.d0;

    // d1 - holds the number of blocks in the device.
    device[device_Id].Number_Of_Blocks = BUSS_ADDRESS.d1;

    logMessage(LOG_OUTPUT_LEVEL, "          ### Number of Sectors: '%i' Number of Blocks: '%i' ###", BUSS_ADDRESS.d0, device[device_Id].Number_Of_Blocks);  
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcopen
// Description  : Open the file for for reading and writing
//
// Inputs       : path - the path/filename of the file to be read
// Outputs      : file handle if successful test, -1 if failure
LcFHandle lcopen(const char *path ) {
    // Log the input peramiters. 
    logMessage(LOG_OUTPUT_LEVEL, "          ### TASK: < OPEN FILE > ");
    logMessage(LOG_OUTPUT_LEVEL, "          ### Path handed to the lopen function '%s' ###", path);  
    
    struct Buss BUSS_ADDRESS;  // Create a object of the structre 

    File_Counter++;

    // Check if device is powered on.
    if (buss_on == 0) {
        logMessage(LOG_OUTPUT_LEVEL, "          ### Powering on the buss ###");
        Power_On(&BUSS_ADDRESS);

        // Allocate the cache
        lcloud_initcache(LC_CACHE_MAXBLOCKS);
        Stats.hits = 0;
        Stats.misses = 0;
        
        // Check if file already open (fail if already open) from a list 
        Lc_Probe_Buss (&BUSS_ADDRESS);

        // Get the device ID.
        for (int i = 0; i < 15; i++) {
            
            // Shift to the righ counting up then return when ever there is a 1
            if (((BUSS_ADDRESS.d0 >> i) & (int)1) == (int)1) {
                logMessage(LOG_OUTPUT_LEVEL, "          ### Device #%i is on the Bus ###", i);

                // assigne the value of the device to the array.
                Number_Of_Devices_On = Device_Counter;
                Device_Counter ++;

                // Create a struct for the device.
                device[i].Power = 1;
                device[i].Number = i;

                // find the ammount of blocks and sectors on that device.
                Lc_Device_Setup(i);
            }

        }
        
    }
    logMessage(LOG_OUTPUT_LEVEL, "          ### Lc Handle number'%i'", File_Counter);

    // If file does not exist, set length to 0
    FILE_HANDLE[File_Counter].Path = *path;
    FILE_HANDLE[File_Counter].length = 0;

    // Return file handle 
    return(File_Counter); 
    
    // if doesn't exsit 
    //return("-1"); // Likely wrong
} 

////////////////////////////////////////////////////////////////////////////////
//
// Function     : Check_Return_Values
// Description  : Check if the device and retrun address was sucessfull.
//
// Inputs       : The LCloudRegisterFrame 64-bit register and a pointer to the buss adress struct.
// Outputs      : 0 - For compleating successfully.
int Check_Return_Values (LCloudRegisterFrame Packed_Registers, struct Buss *BUSS_ADDRESS) {

    // Unpack the registor
    extract_lcloud_registers(Packed_Registers, BUSS_ADDRESS);    

    // Check if the retrun values are correct
    if (BUSS_ADDRESS->b1 == 1) {
        return 0; // Task compleated sucessfully.
    }
    else {
        
        return -1; // The task was un-sucsessfull.
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : Write_block
// Description  : This takes one block (exactly 256), and sends it to the client_lcloud_bus_request. 
//
// Inputs       : The device id, sector and block position to send the block too. Then the buf pointer that holds the data to be written
// Outputs      : the retrun registers.
int Write_block (int Device_ID, int Sector, int Block, char *buf) {

    char buf_Pointer[255];

    memcpy(buf_Pointer, buf, 256);

    // Write to the cache: 
    lcloud_putcache(Device_ID, Sector, Block, buf_Pointer);

    // Create a buss address object for packing.
    struct Buss BUSS_ADDRESS;

    //c2 - LC_XFER_WRITE for write
    BUSS_ADDRESS.b0 = 0;
    BUSS_ADDRESS.b1 = 0;
    BUSS_ADDRESS.c0 = LC_BLOCK_XFER;  // ALWAYS the op code. 
    BUSS_ADDRESS.c1 = Device_ID;
    BUSS_ADDRESS.c2 = LC_XFER_WRITE;
    BUSS_ADDRESS.d0 = Sector;
    BUSS_ADDRESS.d1 = Block;
    
    // Pack the registers.
    uint64_t Packed_Registers = create_lcloud_registers(&BUSS_ADDRESS);

    logMessage(LOG_INFO_LEVEL, "data exsists at: '%x'", buf);

    // Sends the data along with the packed registers to the io buss
    Packed_Registers = client_lcloud_bus_request(Packed_Registers, buf);

    // Check to make sure the retruned register is correct.
    return Check_Return_Values(Packed_Registers, &BUSS_ADDRESS); 
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : Read_Block
// Description  : Takes 1; 256 - bit block from the device 
//
// Inputs       : the device ID, sector, and block of where the data is.
// Outputs      : Then outputs the data into the buf pointer.
void Read_Block (int Device_ID, int Sector, int Block, char *buf) {

    if (device[Device_ID].Number_Of_Sectors == 0) {
        logMessage(LOG_ERROR_LEVEL, " ### ERROR ###: Refenceing Data that does not exsist");
    }
    /////////////////////////
    // Check the cache for the data first
    // Create a buff - pointer to find if the data is there/ NULL if not.
    char * buf_pointer = lcloud_getcache( Device_ID, Sector, Block );

    // Check if the cache came back positive
    if (buf_pointer != NULL && Cache_Enabled == 1) {

        // Calculate the statistics of the data
        Stats.hits += 1;
        
        // Copy the cache data and exit the function.
        memcpy(buf, buf_pointer, 256);
        return;  //<--> UNCOMMMET THIS to use the cache
    }

    // Create a buss address object for packing.
    struct Buss BUSS_ADDRESS;

    //c2 - LC_XFER_WRITE for write
    BUSS_ADDRESS.b0 = 0;
    BUSS_ADDRESS.b1 = 0;
    BUSS_ADDRESS.c0 = LC_BLOCK_XFER;  // ALWAYS the op code. 
    BUSS_ADDRESS.c1 = Device_ID;
    BUSS_ADDRESS.c2 = LC_XFER_READ;
    BUSS_ADDRESS.d0 = Sector; //Sector
    BUSS_ADDRESS.d1 = Block; //Block

    // Pack the registers.
    uint64_t Packed_Registers = create_lcloud_registers(&BUSS_ADDRESS);
    
    // Sends the data along with the packed registers to the io buss
    Packed_Registers = client_lcloud_bus_request(Packed_Registers, buf);  

    // Check to make sure the retruned register is correct.
    Check_Return_Values (Packed_Registers, &BUSS_ADDRESS); 

    // if data is not in cache then fill it
    if (buf_pointer == NULL) {

        // Update hits
        Stats.misses += 1;

        // Write to the cache: 
        lcloud_putcache(Device_ID, Sector, Block, buf);
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcread
// Description  : Read data from the file 
//
// Inputs       : fh - file handle for the file to read from
//                buf - place to put the data
//                len - the length of the read
// Outputs      : number of bytes read, -1 if failure
int lcread( LcFHandle fh, char *buf, size_t len ) {
    logMessage(LOG_OUTPUT_LEVEL, "          ### length handed to the lcread function %i", len);
    logMessage(LOG_OUTPUT_LEVEL, "          ### The read write head's current position is %i", FILE_HANDLE[fh].position);
    
    //memcpy(buf, buf_pointer, 255); // fill the buffer with 0, to make sure that no outside data is getting in

    // Specal case to pretect agest derefrencing
    if (len <= 0) { 
        return 0;
    }

    // Block of data (size 256) sends it to the device. 
    // The device will hold the block in the device. 
    // Check length to see if it is valid
    int Position = FILE_HANDLE[fh].position % 256;

    // Check if file handle valid (is associated with open file)
    if (FILE_HANDLE[fh].Path != 0) {
        logMessage(LOG_OUTPUT_LEVEL, "          ### Device for data: %i'", FILE_HANDLE[fh].block[Position].device);
    }
    else {
        logMessage(LOG_ERROR_LEVEL, "          ### ERROR-404: File hande NON-EXESTANCE");
        return (-1);
    }

    // Figure out what device/sector/block for the data.
    //int Sector_Number = FILE_HANDLE[fh].position / (256*64);
    int Block_Number = FILE_HANDLE[fh].position / 256;

    // Calculate how many blocks the date will take,
    int Number_Of_Blocks = (len + Position) / 256;

    // Find the number of sectors and blocks for the current device.
    int Sectors_In_Device = FILE_HANDLE[fh].block[Block_Number].sector;
    int Blocks_In_Device = FILE_HANDLE[fh].block[Block_Number].block;
    
    // Keep track of what is allready read
    int Allready_Read = 256 - (FILE_HANDLE[fh].position % 256);

    Read_Block (FILE_HANDLE[fh].block[Block_Number].device, Sectors_In_Device, Blocks_In_Device, buf);

    ///////////////
    //memcpy(&Cache[Block_Number][Block_Number][Blocks_In_Device][0], buf, 256);  //<-- Uncomment this
    // Value for the amount of data wrote
    int data_left = len - (256 - Position);

    // Update the buffer length / update the read-write head.
    FILE_HANDLE[fh].position += len;

    memcpy(buf, buf + Position, 256 - Position);  //<-- Uncomment this

    if (Number_Of_Blocks > 0) {
        logMessage(LOG_OUTPUT_LEVEL, "          ### More data needed: Begning recurstion");

        FILE_HANDLE[fh].position -= data_left;
        if (Position == 0){
            
            lcread(fh, buf + 256,  len - Allready_Read );
        }
        else {
            lcread(fh, buf + 256 - Position,  len - Allready_Read );
        }
        
        logMessage(LOG_OUTPUT_LEVEL, "          ### End Of Recursion");
    }

    // Move the buffer position to the start of the data.
    logMessage(LOG_OUTPUT_LEVEL, "          ### Data offset from beginning of block is %i", Position);
    logMessage(LOG_OUTPUT_LEVEL, "          ### Reading from sector: '%i' and block number: '%i'", Sectors_In_Device, Blocks_In_Device);
    logMessage(LOG_OUTPUT_LEVEL, "          ### The number of blocks needed for the read is %i", Number_Of_Blocks + 1);
    logMessage(LOG_OUTPUT_LEVEL, "          ### Read/Write head: '%i'  ", FILE_HANDLE[fh].position);
    logMessage(LOG_OUTPUT_LEVEL, "          ### The Block position is %i", Block_Number);

    //FILE_HANDLE[fh].position -= Allready_Read;
    return(len);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcwrite
// Description  : write data to the file
//
// Inputs       : fh - file handle for the file to write to
//                buf - pointer to data to write
//                len - the length of the write
// Outputs      : number of bytes written if successful test, -1 if failure
int lcwrite( LcFHandle fh, char *buf, size_t len ) {
    
    // Storing what deviece to write to.
    int Using_Device_Number = 0;

    // Find what device to write to.
    for (int i=0; i <= 15; i++) {
        if (device[i].Power == 1) {
            Using_Device_Number = i;
        }
    }

    // Find the number of sectors and blocks for the current device.
    int Sectors_In_Device = device[Using_Device_Number].Number_Of_Sectors;
    int Blocks_In_Device = device[Using_Device_Number].Number_Of_Blocks;
    
    int Position = FILE_HANDLE[fh].position % 256;

    // Find how much data exsists
    int File_Block_Number = FILE_HANDLE[fh].position / 256;  //<-- This number is wrong -- thanks past me UGHHH -- spent 8 hours figurning out somting i allready knew, maybe I should switch to IST?

    // Find the sector, block and position to put the new data. (Continue searching from last placement)
    int Sector_Number = FILE_HANDLE[fh].block[File_Block_Number].sector;
    int Block_Number = FILE_HANDLE[fh].block[File_Block_Number].block;
    
    char new_buf [255] = "";


    logMessage(LOG_OUTPUT_LEVEL, "          ### Writting to device: '%i'  ", Using_Device_Number);
    logMessage(LOG_OUTPUT_LEVEL, "          ### Read/Write head: '%i'  ", FILE_HANDLE[fh].position);
    logMessage(LOG_OUTPUT_LEVEL, "          ### Taking input from: '%i'  ", File_Block_Number);
    logMessage(LOG_OUTPUT_LEVEL, "          ### Writting to sector: '%i' and block number: '%i'", Sector_Number, Block_Number);

    ///////////////////////////
    // If block is not empty; '1' is the 2d array.
    // Check if the block has data.
    if (FILE_HANDLE[fh].block[File_Block_Number].allocated == 1) { //// <- Position > 0 // Gets furhter
        logMessage(LOG_OUTPUT_LEVEL, "          ### There is data in this block -- retreaving old data");

        // Read what is allreading in the block.
        Using_Device_Number = FILE_HANDLE[fh].block[File_Block_Number].device;

        Read_Block (device[Using_Device_Number].Number, FILE_HANDLE[fh].block[File_Block_Number].sector, FILE_HANDLE[fh].block[File_Block_Number].block, new_buf);

    }
    else {

        ///////////////////////////
        // Finding a new empty block to allocate.
        // If another device did not allready take it.
        while (device[Using_Device_Number].Used_Blocks[Sector_Number][Block_Number] != 0) {

            // If the block if used by a diffrent file, get a new block.
            if (Block_Number >= Blocks_In_Device-1) {
                Block_Number = 0;
                
                // If the sector is full, find a new sector
                if (Sector_Number >= Sectors_In_Device-1) {
                    Sector_Number = 0;
                    device[Using_Device_Number].Device_Full = 1;

                    // If device is
                    for (int i=0; i <= 15; i++) {
                        if (device[i].Power == 1 && device[i].Device_Full == 0) {
                            logMessage(LOG_OUTPUT_LEVEL, "          ### Device: '%i' is availible at position %i", device[i].Number, i);
                            Using_Device_Number = i;
                        }
                    }
                    Sectors_In_Device = device[Using_Device_Number].Number_Of_Sectors;
                    Blocks_In_Device = device[Using_Device_Number].Number_Of_Blocks;
                }
                else {
                    Sector_Number++;
                }
            }
            else {
                Block_Number++;
            }
        }
        device[Using_Device_Number].Used_Blocks[Sector_Number][Block_Number] = Using_Device_Number;
        
        if (Block_Number >= Blocks_In_Device){
                Block_Number = 0;
                Sector_Number++;
        }
    }
    logMessage(LOG_OUTPUT_LEVEL, "          ### Writting to device: '%i'  ", Using_Device_Number);
    logMessage(LOG_OUTPUT_LEVEL, "          ### Writting to sector: '%i' and block number: '%i'", Sector_Number, Block_Number);
    
    // Value for the amount of data wrote
    int data_left = len - (256 - Position);

    // # AFTER WRITTING #
    // Add the size of the data to the size of the file. 
    FILE_HANDLE[fh].length += len;
    FILE_HANDLE[fh].position += len;
    
    if (Position + len < 256) {

        // For data less then one block
        memcpy (new_buf + Position, buf, len); 
    }

    else {
        memcpy (new_buf + Position, buf, 256 - Position); 

        // Whole length could not be written, the position must be updatad.
        FILE_HANDLE[fh].length -= data_left;
        FILE_HANDLE[fh].position -= data_left;
    }

    // Call the write block function.
    int status = Write_block(device[Using_Device_Number].Number, Sector_Number, Block_Number, new_buf);
    if (status == -1) {
        logMessage(LOG_ERROR_LEVEL, "           ### The block was written Unsissesfully");
        return -1;  // THE Block was unable to be written.
    } 

    

    ////////////////////////////
    // Write to the current block as much as posible.
    // Create the empty array
    // Update that file to know the blocks position.
    FILE_HANDLE[fh].block[File_Block_Number].device = device[Using_Device_Number].Number;
    FILE_HANDLE[fh].block[File_Block_Number].sector = Sector_Number;
    FILE_HANDLE[fh].block[File_Block_Number].block = Block_Number;
    FILE_HANDLE[fh].block[File_Block_Number].allocated = 1;

    device[Using_Device_Number].Used_Blocks[FILE_HANDLE[fh].block[Block_Number].sector][FILE_HANDLE[fh].block[Block_Number].block] = 1;

    ///////////////////////////
    // for if the size is greater than 256 - position RECURSION!
    if (256 - Position < len) {
        
        // Write the data left in the buffer, using RECURRSION!!
        lcwrite(fh, buf + (256 - Position), data_left);
    }
    
    logMessage(LOG_OUTPUT_LEVEL, "          ### blocks per sector: '%i'  ", Blocks_In_Device);
    return(len);
}



////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcseek
// Description  : Seek to a specific place in the file
//
// Inputs       : fh - the file handle of the file to seek in
//                off - offset within the file to seek to
// Outputs      : position if successful test, -1 if failure
int lcseek( LcFHandle fh, size_t off ) {
    // Changes the pointer. (NO OPPERATIONS BEING DONE.)
    //logMessage(LOG_OUTPUT_LEVEL, "LcHandle handed to the lcseek function %i", fh);
    //logMessage(LOG_OUTPUT_LEVEL, "size handed to the lcseek function %i", off);
    //logMessage(LOG_OUTPUT_LEVEL, "The read write head's current position is %i", FILE_HANDLE[fh].position);
    
    // Check if the offset is greater than the lengh of data. 
    if (off <= FILE_HANDLE[fh].length) {

        // Update the Read/Write head - position pointer.
        FILE_HANDLE[fh].position = off;
        logMessage(LOG_OUTPUT_LEVEL, "          ### The read write head's current position is NOW %i", FILE_HANDLE[fh].position);

        // return success
        return(FILE_HANDLE[fh].position);
    }
    else {
        logMessage(LOG_OUTPUT_LEVEL, "          ### You may not seek past the length of the data!");
        return (-1);
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcclose
// Description  : Close the file
//
// Inputs       : fh - the file handle of the file to close
// Outputs      : 0 if successful test, -1 if failure
int lcclose( LcFHandle fh ) {

    //1. Set the file handels device to 0.
    FILE_HANDLE[fh].Device_Id = 0;


    //3. Check for the return values in the registers for failures, etc.)
    if (FILE_HANDLE[fh].Device_Id != 0) {
        // Device has failed
        return -1;
    }
    else {
        return 0;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcshutdown
// Description  : Shut down the filesystem
//
// Inputs       : none
// Outputs      : 0 if successful test, -1 if failure
int lcshutdown( void ) {

    // Create a buss address object for packing.
    struct Buss BUSS_ADDRESS;

    //1. Pack the registers using your create_lcloud_registers function
    BUSS_ADDRESS.b0 = 0;
    BUSS_ADDRESS.b1 = 0;
    BUSS_ADDRESS.c0 = LC_POWER_OFF;
    BUSS_ADDRESS.c1 = 0;
    BUSS_ADDRESS.c2 = 0;
    BUSS_ADDRESS.d0 = 0;
    BUSS_ADDRESS.d1 = 0;

    LCloudRegisterFrame Packed_Registers = create_lcloud_registers(&BUSS_ADDRESS);
    //2. Set the xfer parameter (NULL for power on)
    void * xfvr = NULL;
    //3. Call the lcloud_io_bus and get the return registers
    Packed_Registers = client_lcloud_bus_request(Packed_Registers, xfvr);
    //4. Unpack the registers using your extract_lcloud_registers function
    extract_lcloud_registers(Packed_Registers, &BUSS_ADDRESS);
    //5. Check for the return values in the registers for failures, etc.)

    // Closing the cache
    lcloud_closecache();

    // Show the hit ratio for the cache accesses.
    logMessage(LOG_INFO_LEVEL, "           ### Cache ###: The hit ratio: '%f' Percent", ((float)Stats.hits)/(((float)Stats.hits + Stats.misses)) * 100 );
    logMessage(LOG_INFO_LEVEL, "           ### Number of hits: '%i'", Stats.hits);
    logMessage(LOG_INFO_LEVEL, "           ### Number of Misses: '%i'", Stats.misses);

    if (BUSS_ADDRESS.b1 != 1) {
        // Device has failed
        return -1;
    }
    else {
        return 0;
    }
}
