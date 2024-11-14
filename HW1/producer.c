/*
CS 420 
Assignment 1: Shared Memory and Multi-Tasking
Group # 21 
Section # 2
OSs Tested on: Linux, Mac

/*
To compile and run your code, make sure that gcc
is installed on your system. As an example, on a
system like Ubunto you can run the command:
    gcc --version
to see if gcc is installed and which version(s).
If it is not installed, usually with a couple of
simple commands you can install it. Like:
    sudo apt update
    sudo apt install build-essential
After you implemented your code(s), you can
compile the codes using commands:
    gcc producer.c -lrt -o producer
    gcc consumer.c -lrt -o consumer
To test it, run it with some sample arguments:
    ./producer 5 100 10
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>

// Size of shared memory block
// Pass this to ftruncate and mmap
#define SHM_SIZE 4096

// Global pointer to the shared memory block
// This should receive the return value of mmap
// Don't change this pointer in any function
void* gShmPtr;

// You won't necessarily need all the functions below
void Producer(int, int, int);
void InitShm(int, int);
void SetBufSize(int);
void SetItemCnt(int);
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
int GetRand(int, int);


int main(int argc, char* argv[])
{
    pid_t pid;
    int bufSize; // Bounded buffer size
    int itemCnt; // Number of items to be produced
    int randSeed; // Seed for the random number generator 

    if(argc != 4){
            printf("Invalid number of command-line arguments\n");
            exit(1);
    }
    bufSize = atoi(argv[1]);
    itemCnt = atoi(argv[2]);
    randSeed = atoi(argv[3]);

    // Write code to check the validity of the command-line arguments

    // Validate command-line arguments
    if (bufSize < 2 || bufSize > 800) {
        printf("Error: bufSize must be between 2 and 800\n");
        exit(1);
    }

    if (itemCnt <= 0) {
        printf("Error: itemCnt must be a positive integer\n");
        exit(1);
    }

    // Function that creates a shared memory segment and initializes its header
    InitShm(bufSize, itemCnt);

    /* fork a child process */ 
    pid = fork();

    if (pid < 0) { /* error occurred */
        fprintf(stderr, "Fork Failed\n");
        exit(1);
    }
    else if (pid == 0) { /* child process */
        printf("Launching Consumer \n");
        execlp("./consumer","consumer",NULL);
    }
    else { /* parent process */
        /* parent will wait for the child to complete */
        printf("Starting Producer\n");
        
        // The function that actually implements the production
        Producer(bufSize, itemCnt, randSeed);
        printf("Producer done and waiting for consumer\n");
        wait(NULL);
        printf("Consumer Completed\n");
    }
    
    return 0;
}


void InitShm(int bufSize, int itemCnt)
{
    int in = 0;
    int out = 0;
    const char *name = "OS_HW1_21"; // Name of shared memory object to be passed to shm_open

    // Write code here to create a shared memory block and map it to gShmPtr
    // Use the above name.
    // **Extremely Important: map the shared memory block for both reading and writing 
    // You can also check if its mapped correct (if ShmPtr is not equal to MAP_FAILED)

    // Write code here to set the values of the four integers in the header
    // Just call the functions provided below, like this


    // Create a shared memory object
    int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        printf("Error creating shared memory object\n");
        exit(1);
    }

    // Set the size of the shared memory object
    ftruncate(shm_fd, SHM_SIZE);

    // Map the shared memory object
    gShmPtr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (gShmPtr == MAP_FAILED) {
        printf("Error mapping shared memory\n");
        exit(1);
    }

    // Initialize the header (buffer size, item count, in, out)
    SetBufSize(bufSize);
    SetItemCnt(itemCnt);
    SetIn(0);
    SetOut(0);

    SetBufSize(bufSize); 	
}

void Producer(int bufSize, int itemCnt, int randSeed)
{
    int in = 0;
    int out = 0;

    srand(randSeed);

    // Write code here to produce itemCnt integer values in the range [0-2500]
    // Use the functions provided below to get/set the values of shared variables "in" and "out"
    // Use the provided function WriteAtBufIndex() to write into the bounded buffer 	
    // Use the provided function GetRand() to generate a random number in the specified range
    // **Extremely Important: Remember to set the value of any shared variable you change locally
    // Use the following print statement to report the production of an item:
    // printf("Producing Item %d with value %d at Index %d\n", i, val, in);
    // where i is the item number, val is the item value, in is its index in the bounded buffer

    srand(randSeed);  // Seed the random number generator

    for (int i = 0; i < itemCnt; i++) 
    {
        int in = GetIn();   // Get current "in" index
        int out = GetOut(); // Get current "out" index
        
        while ((in + 1) % bufSize == out) {
            // Busy-wait until consumer consumes an item
            in = GetIn();  // Update the 'in' index
            out = GetOut(); // Update the 'out' index
        } 

        int val = GetRand(0, 2500);  // Generate a random value
        WriteAtBufIndex(in, val);    // Write the item to the buffer
        
        // Update "in" pointer using circular buffer logic
        SetIn((in + 1) % bufSize);
        
        printf("Producing Item %d with value %d at Index %d\n", i, val, in);
    }
    printf("Producer Completed\n");
}

// Set the value of shared variable "bufSize"
void SetBufSize(int val)
{
    SetHeaderVal(0, val);
}

// Set the value of shared variable "itemCnt"
void SetItemCnt(int val)
{
    SetHeaderVal(1, val);
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
    void* ptr = gShmPtr + 4 * sizeof(int) + indx * sizeof(int);  // Skip the header
    int val;
    memcpy(&val, ptr, sizeof(int));
    return val;
}

// Get a random number in the range [x, y]
int GetRand(int x, int y)
{
    int r = rand();
    r = x + r % (y - x + 1);
    return r;
}