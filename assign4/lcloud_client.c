////////////////////////////////////////////////////////////////////////////////
//
//  File          : lcloud_client.c
//  Description   : This is the client side of the Lion Clound network
//                  communication protocol.
//
//  Author        : Patrick McDaniel
//  Last Modified : Sat 28 Mar 2020 09:43:05 AM EDT
//

// Include Files
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>

// Project Include Files
#include <lcloud_network.h>
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>

//
// Global verables
struct Buss2{
    uint8_t b0 : 4; // b0: 4-bit intiger
    uint8_t b1 : 4; // b1: 4-bit intiger
    uint8_t c0; // c0: 8-bit intiger
    uint8_t c1; // c1: 8-bit intiger
    uint8_t c2; // c3: 8-bit intiger
    uint64_t d0; // d0: 16-bit intiger
    uint64_t d1; // d1: 16-bit intiger
};


struct Socket{
    int socket_handle; 

    char * Defult_IP;
    int Defult_Port;

}File_Socket = {-1};

char * Data_Block;

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : extract_lcloud_c2_c0_registers
// Description  : Extract/decompose the FILE_HANDLE into it's respecing parts.
//
// Inputs       : A 64 bit intiger that holds the packed register.
// Outputs      : A pointer the the BUSS_ADDRESS struct, that holds all 7 peramiters. 
void extract_lcloud_c2_c0_registers(LCloudRegisterFrame resp, struct Buss2 *BUSS_ADDRESS) {

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
// Function     : client_lcloud_bus_request
// Description  : This the client regstateeration that sends a request to the 
//                lion client server.   It will:
//
//                1) if INIT make a connection to the server
//                2) send any request to the server, returning results
//                3) if CLOSE, will close the connection
//
// Inputs       : reg - the request reqisters for the command
//                buf - the block to be read/written from (READ/WRITE)
// Outputs      : the response structure encoded as needed
LCloudRegisterFrame client_lcloud_bus_request( LCloudRegisterFrame reg, void *buf ) {

    // LCLOUD_MAX_BACKLOG = 5
    // LCLOUD_NET_HEADER_SIZE = sizeof(LCloudRegisterFrame)
    logMessage(LOG_INFO_LEVEL, "[lcloud_client.c] #### Talking to Device. ####");

    // If there isn't an open connection already created
    // Use a global variable 'socket_extract_lcloud_c2_c0_registershandle', set initially equal to '-1'.

    // IF 'socket_handle' == -1, there is no open connection.
    if (File_Socket.socket_handle == -1) { 
        logMessage(LOG_INFO_LEVEL, "[lcloud_client.c] There is 'NOT' an Open Connection.");

        // IF there isn't an open connection already created, three things need 
        // to be done.
        //    (a) Setup the address
        //    (b) Create the socket
        //    (c) Create the connection

        // The socket() function creates a file handle for use in communication:
        // • Where,
        // ‣ domain is the communication domain
        // ‣ AF_INET (IPv4), AF_INET6 (IPv6)
        // ‣ type is the communication semantics (stream vs. datagram)
        // ‣ SOCK_STREAM is stream (using TCP by default)
        // ‣ SOCK_DGRAM is datagram (using UDP by default)
        // ‣ protocol selects a protocol from available (not used often)
        if ((File_Socket.socket_handle = socket(PF_INET, SOCK_STREAM, 0)) == -1){
            logMessage(LOG_INFO_LEVEL, "[lcloud_client.c] Incorrect Socket Creation.");
            return -1;
        }
        else {
            logMessage(LOG_INFO_LEVEL, "[lcloud_client.c] Socket Created.");
        }

        // The inet_aton() converts a IPv4 address into the UNIX structure used for processing:
        // • Where,
        // • acmpsc311_util.hress, used in later network communication calls
        // int inet_aton(const char *addr, struct in_addr *inp);
        // CMPSC 311 - Introduction to Systems Programming
        // inet_aton() returns 0 if failure!
        File_Socket.Defult_IP = LCLOUD_DEFAULT_IP;
        File_Socket.Defult_Port = LCLOUD_DEFAULT_PORT;


        struct sockaddr_in v4; // IPv4
        // Setup the address information
        v4.sin_family = AF_INET;
        v4.sin_port = htons(LCLOUD_DEFAULT_PORT);

        if (inet_aton(File_Socket.Defult_IP, &(v4.sin_addr)) == 0){
            logMessage(LOG_INFO_LEVEL, "[lcloud_client.c] Incorrect IP for conversion.");
            return -1;
        }
        else {
            logMessage(LOG_INFO_LEVEL, "[lcloud_client.c] IP System Structure populated correctly.");
        }

        // The connect() system call connects the socket file descriptor to the
        // specified address
        // • Where,
        // • sockfd is the socket (file handle) obtained previously
        // • addr - is the address structure
        // • addlen - is the size of the address structure
        // • Returns 0 if successfully connected, -1 if not
        // CMPSC 311 - Introduction to Systems Programming
        // int connect(int sockfd, const struct sockaddr *addr,
        // socklen_t addrlen);
        if ( connect(File_Socket.socket_handle, (const struct sockaddr *)&v4, sizeof(v4)) == -1 ) {
            logMessage(LOG_INFO_LEVEL, "[lcloud_client.c] Could NOT make a connection.");
            return( -1 );
        }
        else {
            logMessage(LOG_INFO_LEVEL, "[lcloud_client.c] Connection Created.");
        }
    }
    // ELSE, there is an open connection.
    else {
        logMessage(LOG_INFO_LEVEL, "[lcloud_client.c] There 'IS' an Open Connection.");
    }
    
    // Use the helper function you created in assignment #2 to extract the
    // opcode from the provided register 'reg'
    struct Buss2 BUSS_ADDRESS;  // Create a object of the structre 
    extract_lcloud_c2_c0_registers(reg, &BUSS_ADDRESS);
    // There are four cases to consider when extracting this opcode.
    
    ////////////////////////////////////////////////////////////////
    // CASE 1: read operation (look at the c0 and c2 fields)
    // SEND: (reg) <- Network format : send the register reg to the network
    // after converting the register to 'network format'.
    //
    // RECEIVE: (reg) -> Host format
    //          256-byte block (Data read from that frame)
    if (BUSS_ADDRESS.c0 == LC_BLOCK_XFER && BUSS_ADDRESS.c2 == LC_XFER_READ) {
        logMessage(LOG_INFO_LEVEL, "[lcloud_client.c] Reading from device");
        // For sending data use htonll64 (home to network), and ntohll64 respectivly.
        // But do NOT tuch the buffers!
        reg = htonll64(reg);

        // Primitive reading and writing only process only blocks of opaque data:
        // ssize_t write(int fd, const void *buf, size_t count);
        // ssize_t read(int fd, void *buf, size_t count);
        // • Where fd is the file descriptor, buf is an array of bytes to write from or read
        // into, and count is the number of bytes to read or write
        // • The value returned is the number by both is bytes read or written.
        // • Be sure to always check the result
        // • On reads, you are responsible for supplying a buffer that is large enough to
        // put the output into.
        // • look out for memory corruption when buffer is too small …
        write(File_Socket.socket_handle, &reg, sizeof(reg));

        // Convert and read the packed registers
        read(File_Socket.socket_handle, &reg, sizeof(reg));
        reg = ntohll64(reg);


        // Make sure the data that is read is the correct size.
        int Written_Length;
        if ((Written_Length = read(File_Socket.socket_handle, buf, LC_DEVICE_BLOCK_SIZE)) != LC_DEVICE_BLOCK_SIZE) {
            logMessage(LOG_ERROR_LEVEL, "[lcloud_client.c] Read error: '%x'", buf);
            logMessage(LOG_ERROR_LEVEL, "[lcloud_client.c] data: '%s'", buf);
            logMessage(LOG_ERROR_LEVEL, "[lcloud_client.c] Length: '%i'", Written_Length);
            return -1;
        }
    }

    ////////////////////////////////////////////////////////////////
    // CASE 2: write operation (look at the c0 and c2 fields)
    // SEND: (reg) <- Network format : send the register reg to the network 
    // after converting the register to 'network format'.
    //       buf 256-byte block (Data to write to that frame)
    //
    // RECEIVE: (reg) -> Host format
    if (BUSS_ADDRESS.c0 == LC_BLOCK_XFER && BUSS_ADDRESS.c2 == LC_XFER_WRITE) {
        logMessage(LOG_INFO_LEVEL, "[lcloud_client.c] Writting to Device");

        // Convert and pass the packed registers
        reg = htonll64(reg);
        write(File_Socket.socket_handle, &reg, sizeof(reg));

        //char buffer[255];



        // Make sure the data that is read is the correct size.
        int Written_Length;
        Written_Length = write(File_Socket.socket_handle, buf, LC_DEVICE_BLOCK_SIZE);

        
        
        //memset(buf, 0, 256);
        if (Written_Length != LC_DEVICE_BLOCK_SIZE) {
            logMessage(LOG_ERROR_LEVEL, "[lcloud_client.c] Write error: '%x'", buf);
            logMessage(LOG_ERROR_LEVEL, "[lcloud_client.c] data: '%s'", buf);
            logMessage(LOG_ERROR_LEVEL, "[lcloud_client.c] Length: '%i'", Written_Length);
            return -1;
        }

        // Convert and read the packed registers
        read(File_Socket.socket_handle, &reg, sizeof(reg));
        reg = ntohll64(reg);
    }
    

    

    ////////////////////////////////////////////////////////////////
    // CASE 3: Other operations (probes, ...)
    // SEND: (reg) <- Network format : send the register reg to the network 
    // after converting the register to 'network format'.
    //
    // RECEIVE: (reg) -> Host format
    if (BUSS_ADDRESS.c0 != LC_BLOCK_XFER){
        logMessage(LOG_INFO_LEVEL, "[lcloud_client.c] Pinging Device. 'reg' given: '%i'", reg);

        // Convert and pass the packed registers
        reg = htonll64(reg);
        write(File_Socket.socket_handle, &reg, sizeof(reg));

        // Convert and read the packed registers
        read(File_Socket.socket_handle, &reg, sizeof(reg));
        reg = ntohll64(reg);

    }

    ////////////////////////////////////////////////////////////////
    // CASE 4: power off operation
    // SEND: (reg) <- Network format : send the register reg to the network 
    // after converting the register to 'network format'.
    //
    // RECEIVE: (reg) -> Host format
    //
    // Close the socket when finished : reset socket_handle to initial value of -1.
    // close(socket_handle)
    if (BUSS_ADDRESS.c0 == LC_POWER_OFF){

        // Return the socket_handle to -1
        close(File_Socket.socket_handle);
        File_Socket.socket_handle = -1;
    }

    // Return the packed registers
    return reg;
}

