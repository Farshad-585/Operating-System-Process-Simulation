#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

/*  CITS2002 Project 1 2020
    Name:                Farshad Ghanbari                
    Student number(s):   21334883
 */


//  MAXIMUM NUMBER OF PROCESSES OUR SYSTEM SUPPORTS (PID=1..20)
#define MAX_PROCESSES                       20

//  MAXIMUM NUMBER OF SYSTEM-CALLS EVER MADE BY ANY PROCESS
#define MAX_SYSCALLS_PER_PROCESS            50

//  MAXIMUM NUMBER OF PIPES THAT ANY SINGLE PROCESS CAN HAVE OPEN (0..9)
#define MAX_PIPE_DESCRIPTORS_PER_PROCESS    10

//  TIME TAKEN TO SWITCH ANY PROCESS FROM ONE STATE TO ANOTHER
#define USECS_TO_CHANGE_PROCESS_STATE       5

//  TIME TAKEN TO TRANSFER ONE BYTE TO/FROM A PIPE
#define USECS_PER_BYTE_TRANSFERED           1

//  ---------------------------------------------------------------------
//  YOUR DATA STRUCTURES, VARIABLES, AND FUNCTIONS SHOULD BE ADDED HERE:
int timeTaken       = 0;    //  ACCUMULATED EXECUTION TIME ON CPU
int processOnCPU    = 0;    //  PID OF PROCESS CURRENTLY ON CPU
int timeQuantum     = 1000; //  DEFAULT VALUE, CHANGES BY COMMANDLINE ARG..
int pipeSize        = 4096; //  DEFAULT VALUE, CHANGES BY COMMANDLINE ARG..
typedef enum processStatus { NEW, READY, RUNNING,
                             WAITING, SLEEPING, EXIT ,BLOCKED, NONE} processStatus;
typedef enum SysCall       { SYS_COMPUTE, SYS_SLEEP, SYS_EXIT, SYS_FORK , 
                             SYS_WAIT, SYS_PIPE, SYS_WRITEPIPE, SYS_READPIPE} SysCall; 
//  READY QUEUE STRUCTURE (FIFO)               
struct {
    int front;
    int rear;
    int size;
    int queue[MAX_PROCESSES];
} readyQueue;
//  BLOCKED QUEUE STRUCTURE
struct {
    int size;
    int blocked[MAX_PROCESSES];
} blockedQueue;
//  STRUCTURE OF PROCESSES AND THEIR SYSTEM CALLS
struct {
    processStatus status;                      
    int nextSysCall;
    int sleepStart;   // ACCUMULATED TIME OF CPU WHEN PROCESS STARTS SLEEPING  
    int blockedByPID; // Stores the PID of the process that is blocked by  
    int parent;       // ADDED LAST MINUTE FOR EXIT() FUNCTION AND PRINTING PIPES         
    struct {
        int syscall;                
        int usecs;
        int otherPID;
        int pipeDescNum;
        int numBytes;
    } syscalls[MAX_SYSCALLS_PER_PROCESS]; 
    struct {
        int pipeReader;
        int bytesStored;
    } pipeDesc[MAX_PIPE_DESCRIPTORS_PER_PROCESS];
} processes[MAX_PROCESSES];

//  ---------------------------------------------------------------------
//  INITIALISE PROCESSES AND THEIR SYSTEM CALLS
void initialiseProcesses(void){
    for (int p = 0; p < MAX_PROCESSES; ++p){
        processes[p].status      = NONE;
        processes[p].nextSysCall = 0;
        processes[p].sleepStart  = -1;
        processes[p].blockedByPID= -1;
        processes[p].parent      = -1;
        for (int s = 0; s < MAX_SYSCALLS_PER_PROCESS; ++s){
            processes[p].syscalls[s].syscall     = SYS_EXIT;
            processes[p].syscalls[s].usecs       =  0;
            processes[p].syscalls[s].otherPID    = -1;
            processes[p].syscalls[s].pipeDescNum = -1;
            processes[p].syscalls[s].numBytes    = -1;
        }
        for (int i = 0; i < MAX_PIPE_DESCRIPTORS_PER_PROCESS; i++){
            processes[p].pipeDesc[i].bytesStored =  0;
            // -2 MEANS PIPE NOT CREATED YET
            processes[p].pipeDesc[i].pipeReader  = -2;
        }
    }
}
//  INITIALISE READY QUEUE
void initialiseReadyQueue(){
    readyQueue.size  = 0;
    readyQueue.front = 0;
    readyQueue.rear  = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        readyQueue.queue[i] = -1;
    }
}
//  INITIALISE BLOCKED QUEUE
void initialiseBlockedQueue(){
    blockedQueue.size = 0;
    for (int i = 0; i < MAX_PROCESSES; i++){
        blockedQueue.blocked[i] = -1;
    }
}

//  --------------------------------------------------------------------- 
//  FUNCTIONS TO VALIDATE FIELDS IN EACH eventfile - NO NEED TO MODIFY
int check_PID(char word[], int lc)
{
    int PID = atoi(word);

    if(PID <= 0 || PID > MAX_PROCESSES) {
        printf("invalid PID '%s', line %i\n", word, lc);
        exit(EXIT_FAILURE);
    }
    return PID;
}

int check_microseconds(char word[], int lc)
{
    int usecs = atoi(word);

    if(usecs <= 0) {
        printf("invalid microseconds '%s', line %i\n", word, lc);
        exit(EXIT_FAILURE);
    }
    return usecs;
}

int check_descriptor(char word[], int lc)
{
    int pd = atoi(word);

    if(pd < 0 || pd >= MAX_PIPE_DESCRIPTORS_PER_PROCESS) {
        printf("invalid pipe descriptor '%s', line %i\n", word, lc);
        exit(EXIT_FAILURE);
    }
    return pd;
}

int check_bytes(char word[], int lc)
{
    int nbytes = atoi(word);

    if(nbytes <= 0) {
        printf("invalid number of bytes '%s', line %i\n", word, lc);
        exit(EXIT_FAILURE);
    }
    return nbytes;
}

//  parse_eventfile() READS AND VALIDATES THE FILE'S CONTENTS
//  YOU NEED TO STORE ITS VALUES INTO YOUR OWN DATA-STRUCTURES AND VARIABLES
void parse_eventfile(char program[], char eventfile[])
{
#define LINELEN                 100
#define WORDLEN                 20
#define CHAR_COMMENT            '#'

//  ATTEMPT TO OPEN OUR EVENTFILE, REPORTING AN ERROR IF WE CAN'T
    FILE *fp    = fopen(eventfile, "r");

    if(fp == NULL) {
        printf("%s: unable to open '%s'\n", program, eventfile);
        exit(EXIT_FAILURE);
    }
    char    line[LINELEN], words[4][WORDLEN];
    int     lc = 0;

//  READ EACH LINE FROM THE EVENTFILE, UNTIL WE REACH THE END-OF-FILE
    while(fgets(line, sizeof line, fp) != NULL) {
        ++lc;

//  COMMENT LINES ARE SIMPLY SKIPPED
        if(line[0] == CHAR_COMMENT) {
            continue;
        }

//  ATTEMPT TO BREAK EACH LINE INTO A NUMBER OF WORDS, USING sscanf()
        int nwords = sscanf(line, "%19s %19s %19s %19s",
                                    words[0], words[1], words[2], words[3]);

//  WE WILL SIMPLY IGNORE ANY LINE WITHOUT ANY WORDS
        if(nwords <= 0) {
            continue;
        }

//  ENSURE THAT THIS LINE'S PID IS VALID
        int thisPID = check_PID(words[0], lc);

//  OTHER VALUES ON (SOME) LINES
        int otherPID, nbytes, usecs, pipedesc;

//  IDENTIFY LINES RECORDING SYSTEM-CALLS AND THEIR OTHER VALUES
//  THIS FUNCTION ONLY CHECKS INPUT;  YOU WILL NEED TO STORE THE VALUES
        if(nwords == 3 && strcmp(words[1], "compute") == 0) {
            usecs   = check_microseconds(words[2], lc);
            int next = processes[thisPID - 1].nextSysCall;
            processes[thisPID - 1].syscalls[next].syscall = SYS_COMPUTE;
            processes[thisPID - 1].syscalls[next].usecs = usecs;
            processes[thisPID - 1].nextSysCall++;
        }
        else if(nwords == 3 && strcmp(words[1], "sleep") == 0) {
            usecs   = check_microseconds(words[2], lc);
            int next = processes[thisPID - 1].nextSysCall;
            processes[thisPID - 1].syscalls[next].syscall = SYS_SLEEP;
            processes[thisPID - 1].syscalls[next].usecs = usecs;
            processes[thisPID - 1].nextSysCall++;
        }
        else if(nwords == 2 && strcmp(words[1], "exit") == 0) {
            int next = processes[thisPID - 1].nextSysCall;
            processes[thisPID - 1].syscalls[next].syscall = SYS_EXIT;
            processes[thisPID - 1].nextSysCall++;
        }
        else if(nwords == 3 && strcmp(words[1], "fork") == 0) {
            otherPID = check_PID(words[2], lc);
            int next = processes[thisPID - 1].nextSysCall;
            processes[thisPID - 1].syscalls[next].syscall = SYS_FORK;
            processes[thisPID - 1].syscalls[next].otherPID = otherPID -1;
            processes[otherPID- 1].status = NEW;
            processes[otherPID- 1].parent = thisPID - 1;    // ADDED LAST MINUTE
            processes[thisPID - 1].nextSysCall++;
        }
        else if(nwords == 3 && strcmp(words[1], "wait") == 0) {
            otherPID = check_PID(words[2], lc);
            int next = processes[thisPID - 1].nextSysCall;
            processes[thisPID - 1].syscalls[next].syscall = SYS_WAIT;
            processes[thisPID - 1].syscalls[next].otherPID = otherPID -1;
            processes[thisPID - 1].nextSysCall++;
        }
        else if(nwords == 3 && strcmp(words[1], "pipe") == 0) {
            pipedesc = check_descriptor(words[2], lc);
            int next = processes[thisPID - 1].nextSysCall;
            processes[thisPID - 1].syscalls[next].syscall = SYS_PIPE;
            processes[thisPID - 1].syscalls[next].pipeDescNum = pipedesc - 1;
            processes[thisPID - 1].nextSysCall++;
        }
        else if(nwords == 4 && strcmp(words[1], "writepipe") == 0) {
            pipedesc = check_descriptor(words[2], lc);
            nbytes   = check_bytes(words[3], lc);
            int next = processes[thisPID - 1].nextSysCall;
            processes[thisPID - 1].syscalls[next].syscall = SYS_WRITEPIPE;
            processes[thisPID - 1].syscalls[next].pipeDescNum = pipedesc - 1;
            processes[thisPID - 1].syscalls[next].numBytes = nbytes;
            processes[thisPID - 1].nextSysCall++;
        }
        else if(nwords == 4 && strcmp(words[1], "readpipe") == 0) {
            pipedesc = check_descriptor(words[2], lc);
            nbytes   = check_bytes(words[3], lc);
            int next = processes[thisPID - 1].nextSysCall;
            processes[thisPID - 1].syscalls[next].syscall = SYS_READPIPE;
            processes[thisPID - 1].syscalls[next].pipeDescNum = pipedesc - 1;
            processes[thisPID - 1].syscalls[next].numBytes = nbytes;
            processes[thisPID - 1].nextSysCall++;
        }
//  UNRECOGNISED LINE
        else {
            printf("%s: line %i of '%s' is unrecognized\n", program,lc,eventfile);
            exit(EXIT_FAILURE);
        }
    }
    fclose(fp);

#undef  LINELEN
#undef  WORDLEN
#undef  CHAR_COMMENT
}
//  ---------------------------------------------------------------------
//  CONVERTS ENUM INDEX OF PROCESS STATE TO STRING REPRESENTATION
//  USED IN PRINITING DEBUGGING INFORMATION
const char* getStatusString (processStatus status){
    switch (status) {
        case NEW:      return "NEW";      break;
        case READY:    return "READY";    break;
        case RUNNING:  return "RUNNING";  break;
        case WAITING:  return "WAITING";  break;
        case SLEEPING: return "SLEEPING"; break;
        case EXIT:     return "EXIT";     break;
        case BLOCKED:  return "BLOCKED";  break;
        case NONE:     return "NONE";     break;
    }
    return 0;
}
//  ---------------------------------------------------------------------
//  USED TO SWITCH BETWEEN STATES IN SOME SYSTEM CALLS
void stateTransition (int index, processStatus state){
    printf("@%06d\tp%d.%s->%s\n", timeTaken, index + 1, 
    getStatusString(processes[index].status),getStatusString(state));

    processes[index].status = state;
    timeTaken += USECS_TO_CHANGE_PROCESS_STATE;
}
//  ---------------------------------------------------------------------
//  ADDS A PROCESS TO REAR OF THE READY QUEUE
void enqueue (int processIndex){
    
    readyQueue.queue[readyQueue.rear] = processIndex;
    readyQueue.rear += 1;
    readyQueue.size += 1;
}
//  ---------------------------------------------------------------------
//  REMOVES AND RETURNS THE FRONT PROCESS OF THE READY QUEUE
int dequeue (void){
    int front = readyQueue.queue[readyQueue.front];
    readyQueue.front++;
    readyQueue.size--;

    return front;
}
//  ---------------------------------------------------------------------
//  RETURNS THE USECS RECORDED IN SYSCALL OF PROCESSES DATA STRUCTURE
int getusecs (int index){
    return processes[index].syscalls[processes[index].nextSysCall].usecs;
}
//  ---------------------------------------------------------------------
//  RETURNS NUMBER OF BYTES LEFT ON THE SYSTEM CALL
int getSysCallBytes (int index){
    int next = processes[index].nextSysCall;
    return processes[index].syscalls[next].numBytes;
}
//  ---------------------------------------------------------------------
//  RETURNS NUMBER OF BYTES STORED IN THE REQUESTED PIPEDESC
int getPipeBytes (int processPID, int pipeDesc){
    return processes[processPID].pipeDesc[pipeDesc].bytesStored;
}
//  ---------------------------------------------------------------------
//  STORES THE PROVIDED AMOUNT OF BYTES IN THE PIPEDESC
void addBytes (int processPID, int pipeDesc, int amount){
    processes[processPID].pipeDesc[pipeDesc].bytesStored += amount;
}
//  ---------------------------------------------------------------------
//  RETURNS THE USECS RECORDED IN SYSCALL OF PROCESSES DATA STRUCTURE
int getPipeDesc (int processIndex){
    int next = processes[processIndex].nextSysCall;
    return processes[processIndex].syscalls[next].pipeDescNum;
}
//  ---------------------------------------------------------------------
//  RETURNS THE NEXT SYSTEM CALL OF THE PROCESS CURRENTLY ON CPU
SysCall getSysCall (int index){
    int next = processes[index].nextSysCall;
    return processes[index].syscalls[next].syscall;
}
//  ---------------------------------------------------------------------
//  RETUNS TRUE IF THE PROCESS PROVIDED HAS FINISHED SLEEPING
bool finishedSleep (int processIndex){
    return (timeTaken - processes[processIndex].sleepStart) >= getusecs(processIndex);
}
//  ---------------------------------------------------------------------
//  BLOCKED PROCESS IS ADDED TO THE BLOCKED QUEUE
void addToBlockedQueue (int processIndex){
    blockedQueue.blocked[blockedQueue.size] = processIndex;
    blockedQueue.size++;
}
//  ---------------------------------------------------------------------
//  REMOVES THE PROCESS FROM THE BLOCKED QUEUE
void removeFromBlockedQueue(int blockedIndex){
    for (int i = blockedIndex; i < blockedQueue.size - blockedIndex; i++){
        blockedQueue.blocked[i] = blockedQueue.blocked[i + 1];
        blockedQueue.size--;
    }
}
//  ---------------------------------------------------------------------
//  EXECUTES THE EXIT() SYSTEM CALL OF THE PROCESS ON CPU
void processExit (void){
    // CHECK TO SEE IF ANY PROCESS HAS BEEN WAITING FOR THE PROCESS EXITING
    int waitingPID = -1;
    for (int i = 0; i < MAX_PROCESSES; i++){
        int next = processes[i].nextSysCall;
        if (processes[i].syscalls[next].otherPID == processOnCPU){
            waitingPID = i;
            break;
        }
    }
    // IF A PROCESS IS WAITING, THEN WAITING PID WILL NOT BE -1
    if(waitingPID != -1) {
        printf("@%06d\tp%d:exit(), p%d has been waiting for p%d, p%d.WAITING->READY\n",
        timeTaken, processOnCPU + 1, waitingPID + 1, processOnCPU + 1, waitingPID + 1);

        // WAITING PROCESS IS ADDED TO READY QUEUE
        enqueue(waitingPID);
        processes[waitingPID].status = READY;
        timeTaken += USECS_TO_CHANGE_PROCESS_STATE;


        if (processes[processOnCPU].parent != -1) {
            int parent = processes[processOnCPU].parent;
            for (int i = 0; i < MAX_PIPE_DESCRIPTORS_PER_PROCESS; i++){
                // SKIPS PROCESSES THAT DON'T HAVE OPEN PIPES
                if (processes[parent].pipeDesc[i].pipeReader == processOnCPU){
                    printf("\tp%d/%d => [..%d bytes..] => none\n", parent + 1, 
                    i + 1, getPipeBytes(processOnCPU, i));
                    processes[parent].pipeDesc[i].pipeReader = -1; 
                    // FOR PROCESSES WITH PIPES THAT DONT HAVE A READER
                } else if (processes[parent].status == EXIT && 
                    processes[parent].pipeDesc[i].pipeReader == processOnCPU){
                    printf("\tnone => [..empty..] => none\n");
                }
            }
        } else if (processes[processOnCPU].parent == -1){
            for (int i = 0; i < MAX_PIPE_DESCRIPTORS_PER_PROCESS; i++){
                // SKIPS PROCESSES THAT DON'T HAVE OPEN PIPES
                if (processes[processOnCPU].pipeDesc[i].pipeReader != -2){
                    int pipeDescReader = processes[processOnCPU].pipeDesc[i].pipeReader;
                    printf("\tnone => [..%d bytes..] => p%d/%d\n", 
                    getPipeBytes(processOnCPU, i), pipeDescReader + 1, i + 1); 
                    // FOR PROCESSES WITH PIPES THAT DONT HAVE A READER
                } else if (processes[processOnCPU].pipeDesc[i].pipeReader == -1){
                    printf("\tnone => [..empty..] => none\n");
                }
            }
        }


        // PROCESS ON CPU IS EXITED
        printf("@%06d\tp%d.%s->EXITED\n", timeTaken, processOnCPU + 1, 
        getStatusString(processes[processOnCPU].status));

        processes[processOnCPU].status = EXIT;
        timeTaken += USECS_TO_CHANGE_PROCESS_STATE;




        // INCREMENTING THE NEXT SYSTEM CALL OF THE WAITING PROCESS
        processes[waitingPID].nextSysCall++;

     // IF NO PROCESS IS WAITING, EXIT THE PROCESS ON CPU   
    } else {

        if (processes[processOnCPU].parent != -1) {
            int parent = processes[processOnCPU].parent;
            for (int i = 0; i < MAX_PIPE_DESCRIPTORS_PER_PROCESS; i++){
                // SKIPS PROCESSES THAT DON'T HAVE OPEN PIPES
                if (processes[parent].pipeDesc[i].pipeReader == processOnCPU){
                    printf("\tp%d/%d => [..%d bytes..] => none\n", parent + 1, 
                    i + 1, getPipeBytes(processOnCPU, i));
                    processes[parent].pipeDesc[i].pipeReader = -1; 
                    // FOR PROCESSES WITH PIPES THAT DONT HAVE A READER
                } else if (processes[parent].status == EXIT && 
                    processes[parent].pipeDesc[i].pipeReader == processOnCPU){
                    printf("\tnone => [..empty..] => none\n");
                }
            }
        } else if (processes[processOnCPU].parent == -1){
            for (int i = 0; i < MAX_PIPE_DESCRIPTORS_PER_PROCESS; i++){
                // SKIPS PROCESSES THAT DON'T HAVE OPEN PIPES
                if (processes[processOnCPU].pipeDesc[i].pipeReader != -2){
                    int pipeDescReader = processes[processOnCPU].pipeDesc[i].pipeReader;
                    printf("\tnone => [..%d bytes..] => p%d/%d\n", 
                    getPipeBytes(processOnCPU, i), pipeDescReader + 1, i + 1); 
                    // FOR PROCESSES WITH PIPES THAT DONT HAVE A READER
                } else if (processes[processOnCPU].pipeDesc[i].pipeReader == -1){
                    printf("\tnone => [..empty..] => none\n");
                }
            }
        }
        printf("@%06d\tp%d:exit(), p%d.%s->EXITED\n", 
        timeTaken, processOnCPU + 1, processOnCPU + 1, 
        getStatusString(processes[processOnCPU].status));

        processes[processOnCPU].status = EXIT;
        timeTaken += USECS_TO_CHANGE_PROCESS_STATE;

        
    }
}
//  ---------------------------------------------------------------------
//  EXECUTES THE COMPUTE() SYSTEM CALL OF THE PROCESS ON CPU
void processCompute (void){
    if (getusecs(processOnCPU) > timeQuantum){
        // IF PROCESS REQUIRES MORE THAN ACCEPTED TIMEQUANTUM,
        // IT COMPUTES ONLY FOR TIMEQUANTUM AMOUNT
        int next = processes[processOnCPU].nextSysCall;
        processes[processOnCPU].syscalls[next].usecs -= timeQuantum;

        printf("@%06d\tp%d:compute(), for %dusec (%d usecs remaning)\n", 
        timeTaken, processOnCPU + 1, timeQuantum, getusecs(processOnCPU));

        // TOTAL TIME INCREASES BY TIMEQUANTUM
        // PROCESS IS THEN ADDED TO THE READY QUEUE
        timeTaken += timeQuantum;
        stateTransition(processOnCPU, READY);
        enqueue(processOnCPU);
    } else {
        // IF PROCESS REQUIRES LESS THAN ACCEPTED TIMEQUANTUM,
        // IT COMPLETES ITS COMPUTE() PROCESS 
        printf("@%06d\tp%d:compute(), for %dusec (now completed)\n", 
        timeTaken, processOnCPU + 1, getusecs(processOnCPU));

        // TOTAL TIME INCREASES BY THE COMPUTE() AMOUNT
        // PROCESS IS THEN ADDED TO THE READY QUEUE
        timeTaken += getusecs(processOnCPU);
        int next = processes[processOnCPU].nextSysCall;
        processes[processOnCPU].syscalls[next].usecs = 0;
        stateTransition(processOnCPU, READY);
        enqueue(processOnCPU);

        // PROCESS IS THEN READY TO EXECUTE ITS NEXT SYSTEM CALL
        processes[processOnCPU].nextSysCall++;
    }
}
//  ---------------------------------------------------------------------
//  EXECUTES THE FORK() SYSTEM CALL OF THE PROCESS ON CPU
void processFork (void){
    int next = processes[processOnCPU].nextSysCall;
    int child = processes[processOnCPU].syscalls[next].otherPID;

    // CHILD IS ADDED TO READY QUEUE AND TIME IS INCREMENTED
    printf("@%06d\tP%d:fork(), new childPID=%d, P%d.NEW->READY\n", 
    timeTaken, processOnCPU + 1, child + 1, child + 1);
    timeTaken += USECS_TO_CHANGE_PROCESS_STATE;
    processes[child].status = READY;
    enqueue(child);

    // ASSINGING CHILD TO PARENTS' PIPES THAT DONT HAVE A READER
    for (int i = 0; i < MAX_PIPE_DESCRIPTORS_PER_PROCESS; i++){
        if (processes[processOnCPU].pipeDesc[i].pipeReader == -1) {
            processes[processOnCPU].pipeDesc[i].pipeReader = child;
            printf("\tp%d/%d => [..empty..] => p%d/%d\n", 
            processOnCPU + 1, i + 1, child + 1, i + 1);
            // IF PIPE HAS NOT BEEN CREATED (-2) BREAK THE LOOP
            if (processes[processOnCPU].pipeDesc[i].pipeReader == -2){
                break;
            }
        }
    }
    // PARENT ADDED TO READY QUEUE
    stateTransition(processOnCPU, READY);
    enqueue(processOnCPU);
    processes[processOnCPU].nextSysCall++;
}
//  ---------------------------------------------------------------------
//  EXECUTES THE WAIT() SYSTEM CALL OF THE PROCESS ON CPU
void processWait (void){
    // FIND THE PROCESS THAT IT SHOULD WAIT FOR
    int next = processes[processOnCPU].nextSysCall;
    int otherProcess = processes[processOnCPU].syscalls[next].otherPID;
    printf("@%06d\tp%d:wait(), for childPID=%d, P%d.RUNNING->WAITING\n", 
    timeTaken, processOnCPU + 1, otherProcess + 1, processOnCPU + 1);

    // PROCESS STATUS CHANGES TO WAITING AND TIME IS INCREMENTED
    processes[processOnCPU].status = WAITING;   
    timeTaken += USECS_TO_CHANGE_PROCESS_STATE;   
}
//  ---------------------------------------------------------------------
//  EXECUTES THE SLEEP() SYSTEM CALL OF THE PROCESS ON CPU
void processSleep (void){
    printf("@%06d\tp%d:sleep() for %dusecs, p%d.RUNNUNG->SLEEPING\n",
    timeTaken, processOnCPU + 1, getusecs(processOnCPU), processOnCPU + 1);

    // PROCESS STATUS CHANGED TO SLEEPING AND TIME IS INCREMENTED
    processes[processOnCPU].status = SLEEPING;
    timeTaken += USECS_TO_CHANGE_PROCESS_STATE;
    // START OF THE SLEEP TIME IS RECORDED
    processes[processOnCPU].sleepStart = timeTaken;
}
//  ---------------------------------------------------------------------
//  EXECUTES THE pipe() SYSTEM CALL OF THE PROCESS ON CPU
void processPipe(void){
    // FIND WHICH PIPEDESC TO OPEN
    int next = processes[processOnCPU].nextSysCall;
    int pipeToOpen = processes[processOnCPU].syscalls[next].pipeDescNum;

    // OPENNING REQUESTED PIPE. PIPEREADER FROM -2 TO -1;
    processes[processOnCPU].pipeDesc[pipeToOpen].pipeReader = -1;

    // PROCESS IS THEN ADDED TO READY QUEUE
    enqueue(processOnCPU);
    processes[processOnCPU].status = READY;
    timeTaken += USECS_TO_CHANGE_PROCESS_STATE;

    printf("@%06d\tp%d:pipe() pipedesc=%d, p%d.RUNNING->READY\n",
    timeTaken, processOnCPU + 1 , pipeToOpen + 1, processOnCPU + 1);
    printf("\tp%d/%d => [..empty..] => none\n", processOnCPU + 1, pipeToOpen + 1);

    // INCREMENTING FOR NEXT SSYTEM CALL
    processes[processOnCPU].nextSysCall++;
}
//  ---------------------------------------------------------------------
//  EXECUTES THE WRITEPIPE() SYSTEM CALL OF THE PROCESS ON CPU
//  *NOTE*: NUMBER OF LINES IS DOUBLE WHAT IT SHOULD BE TO MATCH DEBUGGING PRINTS
void processWritePipe(void){
    // OBTAINING REQUIRED DATA
    int pipeDescToWrite = getPipeDesc(processOnCPU);
    int numBytesToWrite = getSysCallBytes(processOnCPU);
    int pipeReaderPID   = processes[processOnCPU].pipeDesc[pipeDescToWrite].pipeReader;
    int bytesFreeToWrite = pipeSize - getPipeBytes(processOnCPU, pipeDescToWrite);

    // EXTRA IF STATEMENTS TO MATCH SAMPLE SOLUTION DEBUGGING PRINTS
    // IF THERE IS ENOUGH FREE SPACE TO WRITE
    if (bytesFreeToWrite >= numBytesToWrite && pipeReaderPID == -1) {
        printf("@%06d\tp%d:writepipe() %d bytes to pipedesc=%d (completed), p%d.RUNNING->READY\n", 
        timeTaken, processOnCPU + 1, numBytesToWrite, pipeDescToWrite + 1, processOnCPU + 1);

        // PIPEDESC IS WRITTEN BY THE REQUESTED AMOUNT
        addBytes(processOnCPU, pipeDescToWrite, numBytesToWrite);
        printf("\tp%d/%d => [..%d bytes..] => none\n", processOnCPU + 1, 
        pipeDescToWrite + 1, getPipeBytes(processOnCPU, pipeDescToWrite));

        // TIME INCREMENTED
        timeTaken += (USECS_PER_BYTE_TRANSFERED * numBytesToWrite);

        // NUMBYTES REQUESTED IN SYSCALL IS SUBTRACTED BY THE AMOUNT WRITTEN
        // NUMBYTES REQUESTED IN SYSCALL SHOULD NOW BE 0;
        int next = processes[processOnCPU].nextSysCall;
        processes[processOnCPU].syscalls[next].numBytes -= numBytesToWrite;

        // PROCESS IS ADDED TO READY QUEUE, SYSCALL INCREMENTED
        processes[processOnCPU].status = READY;
        timeTaken += USECS_TO_CHANGE_PROCESS_STATE;
        enqueue(processOnCPU);
        processes[processOnCPU].nextSysCall++;

        // THIS IF STATEMENT IS EXTRA FOR MATCHING PRINT. EXACTLY SIMILAR TO ABOVE.
    }  else if (bytesFreeToWrite >= numBytesToWrite && pipeReaderPID != -1){
        printf("@%06d\tp%d:writepipe() %d bytes to pipedesc=%d (completed), p%d.RUNNING->READY\n", 
        timeTaken, processOnCPU + 1, numBytesToWrite, pipeDescToWrite + 1, processOnCPU + 1);

        // PIPEDESC IS WRITTEN BY THE REQUESTED AMOUNT
        addBytes(processOnCPU, pipeDescToWrite, numBytesToWrite);
        printf("\tp%d/%d => [..%d bytes..] => p%d/%d\n", processOnCPU + 1, pipeDescToWrite + 1, 
        getPipeBytes(processOnCPU, pipeDescToWrite),pipeReaderPID + 1, pipeDescToWrite + 1);

        // TIME INCREMENTED
        timeTaken += (USECS_PER_BYTE_TRANSFERED * numBytesToWrite);

        // NUMBYTES REQUESTED IN SYSCALL IS SUBTRACTED BY THE AMOUNT WRITTEN
        // NUMBYTES REQUESTED IN SYSCALL SHOULD NOW BE 0;
        int next = processes[processOnCPU].nextSysCall;
        processes[processOnCPU].syscalls[next].numBytes -= numBytesToWrite;

        // PROCESS IS ADDED TO READY QUEUE, SYSCALL INCREMENTED
        processes[processOnCPU].status = READY;
        timeTaken += USECS_TO_CHANGE_PROCESS_STATE;
        enqueue(processOnCPU);
        processes[processOnCPU].nextSysCall++;

        // IF NOT ENOUGH SPACE IS LEFT FOR THE TOTAL REQUESTED AMOUNT
    } else if (bytesFreeToWrite < numBytesToWrite && pipeReaderPID == -1){
        printf("@%06d\tp%d:writepipe() %d bytes to pipedesc=%d\n", timeTaken,
        processOnCPU + 1, bytesFreeToWrite, pipeDescToWrite + 1);

        // ONLY WRITE TO PIPEDESC AS MUCH AS FREE SPACE LEFT
        addBytes(processOnCPU, pipeDescToWrite, bytesFreeToWrite);
        printf("\tp%d/%d => [..%d..] => none\n", processOnCPU + 1, pipeDescToWrite + 1, 
        getPipeBytes(processOnCPU, pipeDescToWrite));

        // WRITE IS BLOCKED UNTIL FREE SPACE IS AVAILABLE
        // NUMBYTES REQUESTED IN SYSCALL IS SUBTRACTED BY THE AMOUNT WRITTEN
        int next = processes[processOnCPU].nextSysCall;
        processes[processOnCPU].syscalls[next].numBytes -= bytesFreeToWrite;
        addToBlockedQueue(processOnCPU);
        processes[processOnCPU].status = BLOCKED;
        
        // TIME INCREMENTED FOR STATE TRANSITION AND AMOUNT WRITTEN
        printf("@%06d\tp%d.RUNNUNG->WRITEBLOCKED\n", timeTaken, processOnCPU + 1);
        timeTaken += USECS_TO_CHANGE_PROCESS_STATE;
        timeTaken += (USECS_PER_BYTE_TRANSFERED * bytesFreeToWrite);

        // THIS IF STATEMENT IS EXTRA FOR MATCHING PRINT. EXACTLY SIMILAR TO ABOVE.
    } else if (bytesFreeToWrite < numBytesToWrite && pipeReaderPID != -1){
        printf("@%06d\tp%d:writepipe() %d bytes to pipedesc=%d\n", timeTaken,
        processOnCPU + 1, bytesFreeToWrite, pipeDescToWrite + 1);

        // ONLY WRITE TO PIPEDESC AS MUCH AS FREE SPACE LEFT 
        addBytes(processOnCPU, pipeDescToWrite, bytesFreeToWrite);
        printf("\tp%d/%d => [..%d..] => p%d,%d\n", processOnCPU + 1, pipeDescToWrite + 1, 
        getPipeBytes(processOnCPU, pipeDescToWrite), pipeReaderPID + 1, pipeDescToWrite + 1);
  
        // WRITE IS BLOCKED UNTIL FREE SPACE IS AVAILABLE
        // NUMBYTES REQUESTED IN SYSCALL IS SUBTRACTED BY THE AMOUNT WRITTEN
        int next = processes[processOnCPU].nextSysCall;
        processes[processOnCPU].syscalls[next].numBytes -= bytesFreeToWrite;
        addToBlockedQueue(processOnCPU);
        processes[processOnCPU].status = BLOCKED;
        
        // TIME INCREMENTED FOR STATE TRANSITION AND AMOUNT WRITTEN
        printf("@%06d\tp%d.RUNNUNG->WRITEBLOCKED\n", timeTaken, processOnCPU + 1);
        timeTaken += USECS_TO_CHANGE_PROCESS_STATE;
        timeTaken += (USECS_PER_BYTE_TRANSFERED * bytesFreeToWrite);
    }
}
//  ---------------------------------------------------------------------
//  EXECUTES THE READPIPE() SYSTEM CALL OF THE PROCESS ON CPU
//  *NOTE*: NUMBER OF LINES IS DOUBLE WHAT IT SHOULD BE TO MATCH DEBUGGING PRINTS
void processReadPipe(void){
    // OBTAINING REQUIRED DATA
    int pipeDescToRead = getPipeDesc(processOnCPU);
    int numBytesToRead = getSysCallBytes(processOnCPU);
    int parentProcess  = -1;
    // FIND THE PARENT PROCESS (OWNER OF PIPEDESC) OF THIS CHILD
    for (int i = 0; i < MAX_PROCESSES; i++) {
        for (int j = 0; j < MAX_PIPE_DESCRIPTORS_PER_PROCESS; j++){
            if (processes[i].pipeDesc[j].pipeReader == processOnCPU){
                parentProcess = i;
                break;
            }
        }
    }

    // CASE WHEN PIPE IS EMPTY
    if (getPipeBytes(parentProcess, pipeDescToRead) == 0) {
        printf("@%06d\tp%d:readpipe() from empty pipedesc=%d, p%d.RUNNING->READBLOCKED\n",
        timeTaken, processOnCPU + 1, pipeDescToRead + 1, processOnCPU + 1);

        // PROCESS IS BLOCKED
        processes[processOnCPU].status = BLOCKED;
        timeTaken += USECS_TO_CHANGE_PROCESS_STATE;

        // ADDED TO BLOCKED QUEUE. RECORDING WHICH PROCESS IT IS BLOCKED BY;
        processes[processOnCPU].blockedByPID = parentProcess;
        addToBlockedQueue(processOnCPU);

        // CASE WHEN REQUESTED READ AMOUNT IS LESS THAN PIPESIZE
    } else if (processes[parentProcess].status == EXIT && pipeSize >= numBytesToRead){
        printf("@%06d\tp%d:readpipe() %d bytes to pipedesc=%d (completed), p%d.RUNNING->READY\n", 
        timeTaken, processOnCPU + 1, numBytesToRead, pipeDescToRead + 1, processOnCPU + 1);

        // READING BYTES BY THE REQUESTED AMOUNT
        processes[parentProcess].pipeDesc[pipeDescToRead].bytesStored -= numBytesToRead;
        timeTaken += (USECS_PER_BYTE_TRANSFERED * numBytesToRead);

        // PROCESS ADDED TO READY QUEUE
        processes[processOnCPU].status = READY;
        enqueue(processOnCPU);
        timeTaken += USECS_TO_CHANGE_PROCESS_STATE;

        // SUBTRACTING SYSCALL BYTESTOREAD BY THE READ AMOUNT. SHOULD NOW BE 0;
        printf("\tnone => [..%d bytes..] => p%d/%d\n",
        getPipeBytes(parentProcess, pipeDescToRead), processOnCPU + 1, pipeDescToRead + 1);
        int next = processes[processOnCPU].nextSysCall;
        processes[processOnCPU].syscalls[next].numBytes -= numBytesToRead;

        // INCREMENT FOR NEXT SYSTEM CALL
        processes[processOnCPU].nextSysCall++;

        // THIS IS EXACTLY THE SAME AS ABOVE. REPEATED TO MATCH PRINTS.
    } else if (processes[parentProcess].status != EXIT && pipeSize >= numBytesToRead){
        printf("@%06d\tp%d:readpipe() %d bytes to pipedesk=%d (completed), p%d.RUNNING->READY\n", 
        timeTaken, processOnCPU + 1, numBytesToRead, pipeDescToRead + 1, processOnCPU + 1);

        // READING BYTES BY THE REQUESTED AMOUNT
        processes[parentProcess].pipeDesc[pipeDescToRead].bytesStored -= numBytesToRead;
        timeTaken += (USECS_PER_BYTE_TRANSFERED * numBytesToRead);

        // PROCESS ADDED TO READY QUEUE
        processes[processOnCPU].status = READY;
        enqueue(processOnCPU);
        timeTaken += USECS_TO_CHANGE_PROCESS_STATE;

        // SUBTRACTING SYSCALL BYTESTOREAD BY THE READ AMOUNT. SHOULD NOW BE 0;
        printf("\tp%d/%d => [..%d bytes..] => p%d/%d\n", parentProcess + 1, pipeDescToRead + 1,
        getPipeBytes(parentProcess, pipeDescToRead), processOnCPU + 1, pipeDescToRead + 1);
        int next = processes[processOnCPU].nextSysCall;
        processes[processOnCPU].syscalls[next].numBytes -= numBytesToRead;

        // INCREMENT FOR NEXT SYSTEM CALL
        processes[processOnCPU].nextSysCall++;

        // CASE WHEN NUMBYTES TO READ IS GREATER THAN THE PIPESIZE
    } else if (processes[parentProcess].status == EXIT && pipeSize < numBytesToRead) {
        // FIND OUT HOW MUCH IS AVAILABLE TO READ SO FAR
        int amountToRead = getPipeBytes(parentProcess, pipeDescToRead);

        printf("@%06d\tp%d:readpipe() %d bytes to pipedesc=%d, p%d.RUNNING->READBLOCKED\n", 
        timeTaken, processOnCPU + 1, amountToRead, pipeDescToRead + 1, processOnCPU + 1);

        // READING BYTES BY THE AVAILABLE AMOUNT
        int next = processes[processOnCPU].nextSysCall;
        processes[processOnCPU].syscalls[next].numBytes -= amountToRead;
        processes[parentProcess].pipeDesc[pipeDescToRead].bytesStored -= amountToRead;
        timeTaken += (USECS_PER_BYTE_TRANSFERED * amountToRead);

        // PROCESS IS BLOCKED UNTIL MORE BYTES AVAILABLE TO BE READ
        processes[processOnCPU].status = BLOCKED;
        addToBlockedQueue(processOnCPU);
        timeTaken += USECS_TO_CHANGE_PROCESS_STATE;

        printf("\tnone => [..%d bytes..] => p%d/%d\n",
        getPipeBytes(parentProcess, pipeDescToRead), processOnCPU + 1, pipeDescToRead + 1);

        // THIS IS EXACTLY THE SAME AS ABOVE. REPEATED TO MATCH PRINTS.
    } else if (processes[parentProcess].status != EXIT && pipeSize < numBytesToRead) {
        // FIND OUT HOW MUCH IS AVAILABLE TO READ SO FAR
        int amountToRead = getPipeBytes(parentProcess, pipeDescToRead);

        printf("@%06d\tp%d:readpipe() %d bytes to pipedesc=%d, p%d.RUNNING->READBLOCKED\n", 
        timeTaken, processOnCPU + 1, amountToRead, pipeDescToRead + 1, processOnCPU + 1);

        // READING BYTES BY THE AVAILABLE AMOUNT
        int next = processes[processOnCPU].nextSysCall;
        processes[processOnCPU].syscalls[next].numBytes -= amountToRead;
        processes[parentProcess].pipeDesc[pipeDescToRead].bytesStored -= amountToRead;
        timeTaken += (USECS_PER_BYTE_TRANSFERED * amountToRead);

        // PROCESS IS BLOCKED UNTIL MORE BYTES AVAILABLE TO BE READ
        processes[processOnCPU].status = BLOCKED;
        addToBlockedQueue(processOnCPU);
        timeTaken += USECS_TO_CHANGE_PROCESS_STATE;

        printf("\tp%d/%d => [..%d bytes..] => p%d/%d\n", parentProcess + 1, pipeDescToRead + 1,
        getPipeBytes(parentProcess, pipeDescToRead), processOnCPU + 1, pipeDescToRead + 1);
    }
}
//  ---------------------------------------------------------------------
//  SIMULATES THE BOOTING OF THE SYSTEM AND EXECUTES THE FIRST SYSCALL
void bootSystem (void){
    // AT TIME 0 PROCESS 1 IS RUNNING
    processes[processOnCPU].status = RUNNING;
    printf("@%06d\tBOOT, p%d.RUNNING\n", timeTaken, processOnCPU + 1);
    
    // INITIALISING READY QUEUE AND BLOCKED QUEUE
    initialiseReadyQueue();
    initialiseBlockedQueue();

    // RESET PROCESSES NEXT SYSTEM CALL TO ZERO
    for (int p = 0; p < MAX_PROCESSES; ++p){
        processes[p].nextSysCall = 0;
    }

    // EXECUTE THE FIRST PROCESS ON CPU
    switch(getSysCall(processOnCPU)){
        case SYS_EXIT:       return processExit();        break;
        case SYS_COMPUTE:    return processCompute();     break;
        case SYS_FORK:       return processFork();        break;
        case SYS_SLEEP:      return processSleep();       break;
        case SYS_PIPE:       return processPipe();        break;
        case SYS_READPIPE:   return processReadPipe();    break;
        case SYS_WRITEPIPE:  return processWritePipe();   break;
        case SYS_WAIT:       return processWait();        break;
    }
}
//  ---------------------------------------------------------------------
//  SIMULATES OUR SYSTEM. EXECUTES PROCESS SYSTEM CALLS. INCREMENTS TOTAL TIME.
void runSimulation (int timeQuantum, int pipeSize){
    // AT TIME 0, SYSTEM BOOTS AND FIRST PROCESS IS RUNNING
    // PROCESS 1 EXECUTES ITS FIRST SYSTEM CALL
    bootSystem();
    // INFINIT LOOP RUNS UNTIL NO PROCESS IS RUNNING ON THE CPU
    while (true) {
        
        // CHECK IF THERE ARE ANY PROCESSES ON READY QUEUE
        // IF YES, DEQUEUE THE PROCESS
        if (readyQueue.size > 0){
            //  FRONT PROCESS IS DEQUEUED USING dequeue();
            int dequeuedPID = dequeue();

            //  PROCESS RUNS ON CPU. ITS STATUS CHANGES TO RUNNING;
            processOnCPU = dequeuedPID;
            stateTransition(dequeuedPID, RUNNING);

            //  DEQUEUED PROCESS SYSCALL IS OBTAINED BY getSysCall();
            SysCall nextSystemCall = getSysCall(processOnCPU);

            if (nextSystemCall == SYS_EXIT) {
                processExit();
            } else if (nextSystemCall == SYS_COMPUTE) {
                processCompute();
            } else if (nextSystemCall == SYS_FORK){
                processFork();
            } else if (nextSystemCall == SYS_WAIT){
                processWait();
            } else if (nextSystemCall == SYS_SLEEP){
                processSleep();
            } else if (nextSystemCall == SYS_PIPE){
                processPipe();
            } else if (nextSystemCall == SYS_READPIPE){
                processReadPipe();
            } else if (nextSystemCall == SYS_WRITEPIPE){
                processWritePipe();
            }
        }
        
        // CHECK IF THERE ARE ANY PROCESSES THAT ARE SLEEPING
        for (int i = 0; i < MAX_PROCESSES; i++) {
            // IF NO PROCESS IS IN READY QUEUE, RUN THE SLEEPING PROCESS
            if (processes[i].status == SLEEPING && readyQueue.size == 0){
                // CALCULATE HOW MUCH TIME IS PASSES AND SHOULD BE ADDED
                int timeToAdd = getusecs(i) - (timeTaken - processes[i].sleepStart);
                if (timeToAdd <= 0){
                    timeToAdd = 0;
                } else if (finishedSleep(i)){
                    timeToAdd = 0;
                } else {
                    timeToAdd = timeToAdd;
                }
                printf("@%06d\tp%d finishes sleeping, p%d.SLEEPING->READY\n",
                timeTaken, i + 1, i + 1);
                timeTaken += timeToAdd;
                // PROCESS IS ADDED TO READY QUEUE AND READY FOR NEXT SYSCALL
                processes[i].status = READY;
                enqueue(i);
                timeTaken += USECS_TO_CHANGE_PROCESS_STATE;
                processes[i].nextSysCall++;
                // IF PROCESS HAD FINISHED SLEEPING, IT IS ADDED TO READY QUEUE
            } else if (processes[i].status == SLEEPING && finishedSleep(i)){
                printf("@%06d\tp%d finishes sleeping, p%d.SLEEPING->READY\n", 
                timeTaken, i + 1, i + 1);
                processes[i].status = READY;
                enqueue(i);
                timeTaken += USECS_TO_CHANGE_PROCESS_STATE;
                processes[i].nextSysCall++;
            }
        }

        //  CHECK IF ANY PROCESSES HAVE BEEN BLOCKED
        if (blockedQueue.size > 0){
            // CHECK FOR READBLOCKS
            for (int i = 0; i < blockedQueue.size; i++){
                // OBTAIN REQUIRED DATA
                int blockedProcess = blockedQueue.blocked[i];
                int pipeDescToRead = getPipeDesc(blockedProcess);
                int parentProcess  = processes[blockedProcess].blockedByPID; 
                int next = processes[blockedProcess].nextSysCall;
                int pipeDescToWrite = processes[blockedProcess].syscalls[next].pipeDescNum;
                int bytesStored    = getPipeBytes(parentProcess, pipeDescToRead);

                // IF PIPE REQUESTED IS NOT EMPTY, ADD TO READY QUEUE
                if (getSysCall(blockedProcess) == SYS_READPIPE && bytesStored != 0){
                    printf("@%06d\tp%d can read its pipedesc=%d, p%d.READBLOCK->READY\n",
                    timeTaken, blockedProcess + 1, pipeDescToRead + 1, blockedProcess + 1);

                    removeFromBlockedQueue(i);
                    processes[blockedProcess].status = READY;
                    enqueue(blockedProcess);
                    timeTaken += USECS_TO_CHANGE_PROCESS_STATE;
                    break;
                   //   CHECK WRITE BLOCK 
                } else if (getSysCall(blockedProcess) == SYS_WRITEPIPE && (pipeSize - bytesStored) > 0) {
                    printf("@%06d p%d can write its pipedesc=%d, p%d.WRITEBLOCKED->READY\n", 
                    timeTaken, blockedProcess + 1, pipeDescToWrite + 1, blockedProcess + 1);

                    removeFromBlockedQueue(i);
                    processes[blockedProcess].status = READY;
                    enqueue(blockedProcess);
                    timeTaken += USECS_TO_CHANGE_PROCESS_STATE;
                    break;
                }  
            }

        // IF READY QUEUE IS EMPTY, SIMULATION ENDS
        if (readyQueue.size == 0) {
            break;
        }
        
    }

    //  SIMULATION HALTS
    printf("@%06d\tHALT (no processes alive)\n", timeTaken);
}
//  ---------------------------------------------------------------------
//  CHECK THE COMMAND-LINE ARGUMENTS, CALL parse_eventfile(), RUN SIMULATION
int main(int argc, char *argv[])
{
    timeQuantum = atoi (argv[2]);
    pipeSize = atoi(argv[3]);
    
    // CHECK THE NUMBER OF COMMAND-LINE ARGUMENTS
    if (argc != 4) {
        printf("number of arguments provided is incorrect.\n");
        exit(EXIT_FAILURE);
    }

    // CHECK THAT TIMEQUANTUM IS > 0
    if (timeQuantum < 0){
        printf("Invalid Timequantum enetry\n");
        exit(EXIT_FAILURE);        
    }

    // CHECK THAT PIPESIZE IS > 0
    if (pipeSize < 0){
        printf("Invalid pipsize entry\n");
        exit(EXIT_FAILURE);
    }
    // INITIALISING PROCESSES BEFORE PARSING THE EVENTFILE
    initialiseProcesses();
    parse_eventfile(argv[0], argv[1]);
    // SIMULATION IS RAN
    runSimulation(timeQuantum, pipeSize);   

    // TOTAL SIMULATION TIME        
    printf("timetaken %i\n", timeTaken);
    return 0;
}