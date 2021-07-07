# Operating-System-Process-Simulation
---
### CITS2002 - Systems Programming
#### The University of Western Australia

---
#### Project goals
This project helped me understand some advanced features of the C99 programming language, and how an operating system can manage processes and inter-process communication.

---
#### Description
UNIX-based operating systems employ pre-emptive process scheduling policies to execute hundreds of processes 'simultaneously' on a single CPU. Process take turns to execute on the CPU until they explictly exit, perform an operation that would block (stall) their execution, or until their rationed time on the CPU has elapsed. Only a single process may be 'on' the CPU at one time, and this process is uniquely marked as bing in the Running state. All processes are uniquely identified by a positive integer value termed a process-identifier, or PID. When a UNIX-based operating system has finished booting, only a single process exists (with PID=1) and is executing on the CPU. Like all (future) processes, this initial process may use the CPU to perform some computation or request actions of the operating system kernel by making system-calls.

The processes simulated in this project may only perform a limited number of system-calls, and may only make their system-calls when executing on the CPU. These are described here, along with reference to the possible execution states of the processes.

A process may execute on the CPU for a requested number of microseconds by calling the compute() system-call. The process does not occupy (does not own) the CPU for the requested time, uninterrupted. Instead, its use of the CPU is interleaved with other processes in a pre-emptive manner. For example, consider a computer system with a scheduling-quantum of 1000 microseconds. If a process requests to compute (on the CPU) for 3200 microseconds, it will first occupy the CPU for 1000 microseconds, leave the CPU, be marked as Ready while other process(es) execute, occupy the CPU for another 1000 microseconds, and so on..., until its final turn on the CPU (for the remaining 200 microseconds). It will then be marked as Ready, ready to make its next system-call when it next runs on the CPU.

A process may relinquish its use of the CPU by calling the sleep() system-call, indicating for how many microseconds it wishes to sleep. The process will leave the CPU, be marked as Sleeping, and will not execute again until at least that time has passed.

A process may request its own termination by calling the exit() system-call. When a process terminates, its resources (if any?) are deallocated, any pipes that it had opened for reading or writing are closed (see later), and its PID may be re-used (re-assigned) for future processes.

A process may request to be informed of the termination of another process by calling the wait() system-call. Until the process being waited for terminates the calling process will be marked as Waiting. When the process being waited for eventually terminates, the calling process is marked as Ready.

A process may create a new process by calling the fork() system-call. The process making the fork() system-call is termed the parent process, and the newly created process is termed the child process. The new child process is initialised with a copy of all attributes of the parent process, except that its cumulative execution time is set to zero, and it receives the next available (unused) PID.

A process may request a new inter-process communication buffer termed a pipe(). A pipe is a unidirectional in-memory array of bytes (of finite size). Pipes connect two processes - one process may write data to the pipe, and the other may read data from it. Each process has a limited number of pipe-descriptors - non-negative integers which refer to either the 'writing end' or the 'reading end' of a pipe. When a process creates a new pipe by calling the pipe() system-call, the creating process immediately becomes the writer of the pipe. When a process forks a new (child) process, the new child process immediately becomes the reader of the pipe. When a pipe has both a writer and a reader, the two processes may communicate by writing to and reading from the pipe.

A process may write or read data to a pipe using the writepipe() or readpipe() system-calls. Pipes may hold a fixed amount of data (typically 4KB).

A process attempting to write more data than will fit in the pipe will write some (possibly none) of its data to the pipe, and then block until some space becomes available in the pipe. A writing process will remain blocked until all of its writepipe() request has been written to the pipe after which it will be marked as Ready again.

A process attempting to read from an empty pipe will block until some bytes are available in the pipe. A reading process will remain blocked until all of its readpipe() request has been read from the pipe after which it will be marked as Ready again.

Note: the implementation and operation of pipes required for this project was slightly different, and much simpler, than 'true' pipes in a UNIX-based operating system.
---
#### Event Files
An eventfile is a simple text file containing the historic record of the system-calls requested by the processes of a simple computer system. After the computer system has booted, only a single process (with PID=1) will be running. Thus the first line of every eventfile is a system-call request by PID=1, and no other processes will appear until that first process performs a fork(). The very last line of an eventfile will record the last exit() call, after which no processes will be running (and the system halts). Each line of the file consists of a number of white-space separated words. The first word is always a positive PID, indicating which process is performing the action described on the remainder of the line. The second word on each line is always the name of a system-call requested by the process. Some lines will also have one or two additional words which further describe input paramete(s) for the system-call, or the value returned by the system-call. The supported system-calls are:

  - compute() microseconds-required-on-CPU
  - sleep() microseconds-to-sleep
  - exit()
  - wait() PID-to-wait-for
  - fork() PID-of-new-child-process
  - pipe() new-writing-pipe-descriptor
  - writepipe() writing-pipe-descriptor number-of-bytes
  - readpipe() reading-pipe-descriptor number-of-bytes

Starting code was provided so that we could parse the information in the eventfiles. Then we stored this information in our own data structres and varaibles before we commenced the simulation.

---
NOTE: THIS PROJECT WAS PART OF THE SYSTEMS PROGRAMMING UNIT(CITS2002) AT THE UNIVERSITY OF WESTERN AUSTRALIA. SOME OF THE CODE AND LOGIC WAS PROVIDED BY PROFESSOR CHRIS MCDONALD. 
