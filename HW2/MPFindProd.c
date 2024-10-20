/*
CS 420 
Assignment 2: Multiprocessing and Synchronization
Group # 2
Section # 2
OSs Tested on: Linux, Ubuntu, Mac, etc., along with your system's CPU specifications
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/timeb.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_SIZE 100000000
#define MAX_PROCESSES 16
#define RANDOM_SEED 8631
#define MAX_RANDOM_NUMBER 3000
#define NUM_LIMIT 9973

// Global variables
long gRefTime; //For timing
int *gData; // The array that will hold the data

int *gProcessProd; // Shared memory for each process's result
int *gDoneProcessCount; // Shared memory for counting done processes

// Semaphores
sem_t *mutex; // Semaphore for mutual exclusion

int SqFindProd(int size); //Sequential FindProduct (no threads) computes the product of all the elements in the array mod NUM_LIMIT
void ProcessFindProdWithSemaphore(int start, int end, int processNum); // Process function with semaphores
int ComputeTotalProduct(); // Multiply the division products to compute the total modular product 
void InitSharedVars(int processCount); // Initialize shared variables
void GenerateInput(int size); // Generate input array
void CalculateIndices(int arraySize, int processCount, int indices[MAX_PROCESSES][3]); //Calculate the indices to divide the array into T divisions, one division per thread
int GetRand(int min, int max);//Get a random number between min and max

// Timing functions
long GetMilliSecondTime(struct timeb timeBuf);
long GetCurrentTime(void);
void SetTime(void);
long GetTime(void);

int main(int argc, char *argv[]) {
    int indices[MAX_PROCESSES][3]; // Array divisions
    int i, processCount, arraySize, prod;

	// Code for parsing and checking command-line arguments
	if(argc != 3) {
        fprintf(stderr, "Invalid number of arguments!\n");
        exit(-1);
    }
    if((arraySize = atoi(argv[1])) <= 0 || arraySize > MAX_SIZE) {
        fprintf(stderr, "Invalid Array Size\n");
        exit(-1);
    }
    processCount = atoi(argv[2]);
    if(processCount > MAX_PROCESSES || processCount <= 0) {
        fprintf(stderr, "Invalid Process Count\n");
        exit(-1);
    }

    // Shared memory allocation
    int shmDataId = shmget(IPC_PRIVATE, sizeof(int) * arraySize, IPC_CREAT | 0666);
    int shmProdId = shmget(IPC_PRIVATE, sizeof(int) * processCount, IPC_CREAT | 0666);
    int shmDoneId = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    int shmMutexId = shmget(IPC_PRIVATE, sizeof(sem_t), IPC_CREAT | 0666);

    // Attach shared memory
    gData = shmat(shmDataId, NULL, 0);
    gProcessProd = shmat(shmProdId, NULL, 0);
    gDoneProcessCount = shmat(shmDoneId, NULL, 0);
    mutex = shmat(shmMutexId, NULL, 0);

    GenerateInput(arraySize);

    CalculateIndices(arraySize, processCount, indices);

    // Sequential product computation
    SetTime();
    prod = SqFindProd(arraySize);
    printf("Sequential multiplication completed in %ld ms. Product = %d\n", GetTime(), prod);

    // Multi-process computation with parent waiting for all children
    InitSharedVars(processCount);
    SetTime();
	
	// Write your code here
	// Initialize threads, create threads, and then let the parent wait for all threads using pthread_join
	// The thread start function is ThFindProd
	// Don't forget to properly initialize shared variables

	// Loop to create child processes
	for (i = 0; i < processCount; i++) {
		pid_t pid = fork(); // Fork new process
		if (pid == 0) { // If child process
			ProcessFindProd(indices[i][1], indices[i][2], i); // computes the product
			exit(0);
		}
	}

	// Loop to wait for all child processes to complete
	for (i = 0; i < processCount; i++) {
		wait(NULL); // Parent process waits for each child to finish
	}

    prod = ComputeTotalProduct(processCount);
    printf("Process multiplication with parent waiting for all children completed in %ld ms. Product = %d\n", GetTime(), prod);

    // Multi-process computation with busy waiting
    SetTime();

	// Write your code here
	// Don't use any semaphores in this part
	// Initialize threads, create threads, and then make the parent continually check on all child threads
	// The thread start function is ThFindProd
	// Don't forget to properly initialize shared variables

    for (i = 0; i < processCount; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            ProcessFindProd(indices[i][1], indices[i][2], i);
            exit(0);
        }
    }
    // Busy waiting for all processes to complete
    while (*gDoneProcessCount < processCount) { /* Busy waiting */ }
    prod = ComputeTotalProduct(processCount);
    printf("Process multiplication with parent busy waiting completed in %ld ms. Product = %d\n", GetTime(), prod);

    // Multi-process computation with semaphores
    SetTime();

	// Write your code here
	// Initialize threads, create threads, and then make the parent wait on the "completed" semaphore
	// The thread start function is ThFindProdWithSemaphore
	// Don't forget to properly initialize shared variables and semaphores using sem_init
	
    for (i = 0; i < processCount; i++) {
        pid_t pid = fork();
		// If child
		if (pid == 0) {
            ProcessFindProdWithSemaphore(indices[i][1], indices[i][2], i);
            exit(0);
        }
    }
    while (*gDoneProcessCount < processCount) { /* Busy waiting */ }
    prod = ComputeTotalProduct(processCount);
    printf("Process multiplication with semaphore synchronization completed in %ld ms. Product = %d\n", GetTime(), prod);

    // Cleanup shared memory
    shmctl(shmDataId, IPC_RMID, NULL);
    shmctl(shmProdId, IPC_RMID, NULL);
    shmctl(shmDoneId, IPC_RMID, NULL);
    shmctl(shmMutexId, IPC_RMID, NULL);

    return 0;
}

// Write a regular sequential function to multiply all the elements in gData mod NUM_LIMIT
// REMEMBER TO MOD BY NUM_LIMIT AFTER EACH MULTIPLICATION TO PREVENT YOUR PRODUCT VARIABLE FROM OVERFLOWING
int SqFindProd(int size) {
    int prod = 1;
    for (int i = 0; i < size; i++) {
        prod *= gData[i];
        prod %= NUM_LIMIT;
    }
    return prod;
}


// Write a thread function that computes the product of all the elements in one division of the array mod NUM_LIMIT
// REMEMBER TO MOD BY NUM_LIMIT AFTER EACH MULTIPLICATION TO PREVENT YOUR PRODUCT VARIABLE FROM OVERFLOWING
// When it is done, this function should store the product in gThreadProd[threadNum] and set gThreadDone[threadNum] to true
void ProcessFindProd(int start, int end, int processNum) {
    gProcessProd[processNum] = 1;
    for (int i = start; i <= end; i++) {
        gProcessProd[processNum] *= gData[i];
        gProcessProd[processNum] %= NUM_LIMIT;
    }
    (*gDoneProcessCount)++; // Indicate completion
}

// Write a process function that computes the product of all the elements in one division of the array mod NUM_LIMIT
// REMEMBER TO MOD BY NUM_LIMIT AFTER EACH MULTIPLICATION TO PREVENT YOUR PRODUCT VARIABLE FROM OVERFLOWING
// When it is done, this function should store the product in gProcessProd[processNum] and increment gDoneProcessCount to indicate completion
void ProcessFindProd(int start, int end, int processNum) {
    gProcessProd[processNum] = 1; // Initialize
    for (int i = start; i <= end; i++) {
        gProcessProd[processNum] *= gData[i]; // Multiply element
        gProcessProd[processNum] %= NUM_LIMIT; // Mod by NUM_LIMIT to avoid overflow
    }
    (*gDoneProcessCount)++; // Atomically increment
}

// Write a thread function that computes the product of all the elements in one division of the array mod NUM_LIMIT
// REMEMBER TO MOD BY NUM_LIMIT AFTER EACH MULTIPLICATION TO PREVENT YOUR PRODUCT VARIABLE FROM OVERFLOWING
// When it is done, this function should store the product in gThreadProd[threadNum]
// If the product value in this division is zero, this function should post the "completed" semaphore
// If the product value in this division is not zero, this function should increment gDoneThreadCount and
// post the "completed" semaphore if it is the last thread to be done
// Don't forget to protect access to gDoneThreadCount with the "mutex" semaphore
void ProcessFindProdWithSemaphore(int start, int end, int processNum) {
    gProcessProd[processNum] = 1; // Initalize
    for (int i = start; i <= end; i++) {
        gProcessProd[processNum] *= gData[i]; // Multiply
        gProcessProd[processNum] %= NUM_LIMIT; // Mod
    }
	// Protect access by using semaphore
    sem_wait(mutex); 
    (*gDoneProcessCount)++; // Atomically increment
    sem_post(mutex);
}

int ComputeTotalProduct(int processCount) {
    int prod = 1;
    for (int i = 0; i < processCount; i++) {
        prod *= gProcessProd[i];
        prod %= NUM_LIMIT;
    }
    return prod;
}

void InitSharedVars(int processCount) {
    *gDoneProcessCount = 0;
    for (int i = 0; i < processCount; i++) {
        gProcessProd[i] = 1;
    }
    sem_init(mutex, 1, 1); // Initialize the semaphore
}

// Write a function that fills the gData array with random numbers between 1 and MAX_RANDOM_NUMBER
// If indexForZero is valid and non-negative, set the value at that index to zero
void GenerateInput(int size) {
    srand(RANDOM_SEED);
    for (int i = 0; i < size; i++) {
        gData[i] = GetRand(1, MAX_RANDOM_NUMBER);
    }
}

// Write a function that calculates the right indices to divide the array into thrdCnt equal divisions
// For each division i, indices[i][0] should be set to the division number i,
// indices[i][1] should be set to the start index, and indices[i][2] should be set to the end index
void CalculateIndices(int arraySize, int processCount, int indices[MAX_PROCESSES][3]) {
    int chunkSize = arraySize / processCount;
    for (int i = 0; i < processCount; i++) {
        indices[i][0] = i; // Process number
        indices[i][1] = i * chunkSize; // Start index
        indices[i][2] = (i == processCount - 1) ? arraySize - 1 : (i + 1) * chunkSize - 1; // End index
    }
}

// Get a random number in the range [x, y]
int GetRand(int x, int y) {
    int r = rand();
    r = x + r % (y-x+1);
    return r;
}

long GetMilliSecondTime(struct timeb timeBuf){
	long mliScndTime;
	mliScndTime = timeBuf.time;
	mliScndTime *= 1000;
	mliScndTime += timeBuf.millitm;
	return mliScndTime;
}

long GetCurrentTime(void){
	long crntTime=0;
	struct timeb timeBuf;
	ftime(&timeBuf);
	crntTime = GetMilliSecondTime(timeBuf);
	return crntTime;
}

void SetTime(void){
	gRefTime = GetCurrentTime();
}

long GetTime(void){
	long crntTime = GetCurrentTime();
	return (crntTime - gRefTime);
}