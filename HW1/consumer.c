/*
CS 420 
Assignment 1: Shared Memory and Multi-Tasking
Group # 21 
Section # 2
OSs Tested on: Linux, Mac
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>

// Size of shared memory block
// Pass this to ftruncate and mmap
#define SHM_SIZE 4096

// Global pointer to the shared memory block
// This should receive the return value of mmap
// Don't change this pointer in any function
void* gShmPtr;

// You won't necessarily need all the functions below
void SetIn(int);
void SetOut(int);
void SetHeaderVal(int, int);
int GetBufSize();
int GetItemCnt();
int GetIn();
int GetOut();
int GetHeaderVal(int);
void WriteAtBufIndex(int, int);
int ReadAtBufIndex(int);

int main()
{
    const char *name = "OS_HW1_21"; // Name of shared memory block to be passed to shm_open
    int bufSize; // Bounded buffer size
    int itemCnt; // Number of items to be consumed
    int in; // Index of next item to produce
    int out; // Index of next item to consume

    // Write code here to create a shared memory block and map it to gShmPtr
    // Use the name above
    // **Extremely Important: map the shared memory block for both reading and writing 
    // Use PROT_READ | PROT_WRITE

    // Create a shared memory block and map it to gShmPtr

    // Open the shared memory object
    int shm_fd = shm_open(name, O_RDWR, 0666); 
    if (shm_fd == -1) {
        printf("Error: Could not open shared memory object\n");
        exit(1);
    }
    
    // Map the shared memory
    gShmPtr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0); 
    if (gShmPtr == MAP_FAILED) {
        printf("Error: Could not map shared memory\n");
        exit(1);
    }

    // Initialize in & out
    in = GetIn();
    out = GetOut();


    // Write code here to read the four integers from the header of the shared memory block 
    // These are: bufSize, itemCnt, in, out
    // Just call the functions provided below like this:
    bufSize = GetBufSize();

    // Read total items to consume
    itemCnt = GetItemCnt();    
    while (in == out) {
        // Busy-wait until producer produces an item
        // Update the 'in' index
        in = GetIn();  
        // Update the 'out' index
        out = GetOut(); 
    }

    // Write code here to check that the consumer has read the right values: 

    for (int i = 0; i < itemCnt; i++) {
        do {
            in = GetIn();
            out = GetOut();
        } 
        // Wait until there is at least one item in the buffer
        while (in == out); 

        int val = ReadAtBufIndex(out);
        // Circulur buffer
        SetOut((out + 1) % bufSize);
        printf("Consuming Item %d with value %d at Index %d\n", i, val, out);
    }

    printf("Consumer reading: bufSize = %d\n",bufSize);

    // Write code here to consume all the items produced by the producer
    // Use the functions provided below to get/set the values of shared variables in, out, bufSize
    // Use the provided function ReadAtBufIndex() to read from the bounded buffer 	
    // **Extremely Important: Remember to set the value of any shared variable you change locally
    // Use the following print statement to report the consumption of an item:
    // printf("Consuming Item %d with value %d at Index %d\n", i, val, out);
    // where i is the item number, val is the item value, out is its index in the bounded buffer
    
    
    // remove the shared memory segment 
    if (shm_unlink(name) == -1) {
        printf("Error removing %s\n",name);
        exit(-1);
    }

    return 0;
}


// Set the value of shared variable "in"
void SetIn(int val)
{
    SetHeaderVal(2, val);
}

// Set the value of shared variable "out"
void SetOut(int val)
{
    SetHeaderVal(3, val);
}

// Get the ith value in the header
int GetHeaderVal(int i)
{
    int val;
    void* ptr = gShmPtr + i * sizeof(int);
    memcpy(&val, ptr, sizeof(int));
    return val;
}

// Set the ith value in the header
void SetHeaderVal(int i, int val) {
    void* ptr = gShmPtr + i * sizeof(int);
    memcpy(ptr, &val, sizeof(int));
}

// Get the value of shared variable "bufSize"
int GetBufSize()
{       
    return GetHeaderVal(0);
}

// Get the value of shared variable "itemCnt"
int GetItemCnt()
{
    return GetHeaderVal(1);
}

// Get the value of shared variable "in"
int GetIn()
{
    return GetHeaderVal(2);
}

// Get the value of shared variable "out"
int GetOut()
{             
    return GetHeaderVal(3);
}


// Write the given val at the given index in the bounded buffer 
void WriteAtBufIndex(int indx, int val)
{
    // Skip the four-integer header and go to the given index 
    void* ptr = gShmPtr + 4 * sizeof(int) + indx * sizeof(int);
    memcpy(ptr, &val, sizeof(int));
}

// Read the val at the given index in the bounded buffer
int ReadAtBufIndex(int indx) {
    // Skip the header
    void* ptr = gShmPtr + 4 * sizeof(int) + indx * sizeof(int);  
    int val;
    memcpy(&val, ptr, sizeof(int));
    return val;
}

