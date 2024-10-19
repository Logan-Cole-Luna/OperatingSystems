/*
CS 420 
Assignment 2: Multiprocessing and Synchronization
Group # <- just your group number in this line
Section # <- just your section number
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

// Global variables in shared memory
int *gData; // The array that will hold the data
int *gProcessProd; // Shared memory for each process's result
int *gDoneProcessCount; // Shared memory for counting done processes
sem_t *mutex; // Semaphore for mutual exclusion

// Function prototypes
int SqFindProd(int size); // Sequential FindProduct (no processes)
void ProcessFindProd(int start, int end, int processNum); // Process function without semaphores
void ProcessFindProdWithSemaphore(int start, int end, int processNum); // Process function with semaphores
int ComputeTotalProduct(int processCount); // Multiply the division products to compute the total modular product
void InitSharedVars(int processCount); // Initialize shared variables
void GenerateInput(int size); // Generate input array
void CalculateIndices(int arraySize, int processCount, int indices[MAX_PROCESSES][3]); // Calculate array divisions
int GetRand(int min, int max); // Get a random number

// Timing functions
long GetMilliSecondTime(struct timeb timeBuf);
long GetCurrentTime(void);
void SetTime(void);
long GetTime(void);

// Timing reference
long gRefTime;

int main(int argc, char *argv[]) {
    int indices[MAX_PROCESSES][3]; // Array divisions
    int i, processCount, arraySize, prod;

    // Argument parsing
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

    // Generate input array
    GenerateInput(arraySize);
    // Calculate array divisions
    CalculateIndices(arraySize, processCount, indices);

    // Initialize shared variables
    InitSharedVars(processCount);

    // Sequential product computation
    SetTime();
    prod = SqFindProd(arraySize);
    printf("Sequential multiplication completed in %ld ms. Product = %d\n", GetTime(), prod);

    // Multi-process computation with parent waiting for all children
    SetTime();
    for (i = 0; i < processCount; i++) {
        pid_t pid = fork();
        if (pid == 0) { // Child process
            ProcessFindProd(indices[i][1], indices[i][2], i);
            exit(0); // Child process exits after work
        }
    }
    for (i = 0; i < processCount; i++) {
        wait(NULL); // Parent waits for all children to complete
    }
    prod = ComputeTotalProduct(processCount);
    printf("Process multiplication with parent waiting for all children completed in %ld ms. Product = %d\n", GetTime(), prod);

    // Multi-process computation with busy waiting
    SetTime();
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
    for (i = 0; i < processCount; i++) {
        pid_t pid = fork();
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

// Sequential product computation
int SqFindProd(int size) {
    int prod = 1;
    for (int i = 0; i < size; i++) {
        prod *= gData[i];
        prod %= NUM_LIMIT;
    }
    return prod;
}

// Process computation without semaphore synchronization
void ProcessFindProd(int start, int end, int processNum) {
    gProcessProd[processNum] = 1;
    for (int i = start; i <= end; i++) {
        gProcessProd[processNum] *= gData[i];
        gProcessProd[processNum] %= NUM_LIMIT;
    }
    (*gDoneProcessCount)++; // Indicate completion
}

// Process computation with semaphore synchronization
void ProcessFindProdWithSemaphore(int start, int end, int processNum) {
    gProcessProd[processNum] = 1;
    for (int i = start; i <= end; i++) {
        gProcessProd[processNum] *= gData[i];
        gProcessProd[processNum] %= NUM_LIMIT;
    }
    sem_wait(mutex);
    (*gDoneProcessCount)++;
    sem_post(mutex);
}

// Multiply the partial products from each process
int ComputeTotalProduct(int processCount) {
    int prod = 1;
    for (int i = 0; i < processCount; i++) {
        prod *= gProcessProd[i];
        prod %= NUM_LIMIT;
    }
    return prod;
}

// Initialize shared variables
void InitSharedVars(int processCount) {
    *gDoneProcessCount = 0;
    for (int i = 0; i < processCount; i++) {
        gProcessProd[i] = 1;
    }
    sem_init(mutex, 1, 1); // Initialize the semaphore
}

// Generate the input array
void GenerateInput(int size) {
    srand(RANDOM_SEED);
    for (int i = 0; i < size; i++) {
        gData[i] = GetRand(1, MAX_RANDOM_NUMBER);
    }
}

// Calculate the indices to divide the array for each process
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
    return x + r % (y - x + 1);
}

// Timing functions
long GetMilliSecondTime(struct timeb timeBuf) {
    return timeBuf.time * 1000 + timeBuf.millitm;
}

long GetCurrentTime(void) {
    struct timeb timeBuf;
    ftime(&timeBuf);
    return GetMilliSecondTime(timeBuf);
}

void SetTime(void) {
    gRefTime = GetCurrentTime();
}

long GetTime(void) {
    return GetCurrentTime() - gRefTime;
}