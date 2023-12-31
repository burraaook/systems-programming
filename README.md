# Systems Programming  
University assignments of Systems Programming course which are about POSIX, File I/O, buffering, file events, signals, process creation, process termination, process control, process synchronization, process communication, orphans, zombies, pipes, FIFOs, shared memory, semaphores, threads, condition variables, mutexes, sockets using C programming language.  

| Assignment | Topic | Description |
| --- | --- | --- |
| [Custom Dups](https://github.com/burraaook/systems-programming/tree/main/01_custom-dups) | File I/O, POSIX, Redirection, Race Condition | Reads number of bytes from command line arguments writes it using lseek before each write, two process run same time and writes to same file, results compared. Implement dup, dup2 using fcntl. |
| [Terminal Emulator](https://github.com/burraaook/systems-programming/tree/main/02_terminal-emulator) | Process Creation, Signal Handling, Process Termination, Child Processes, Pipes, Redirections, Error Handling | Implementing a terminal emulator capable of handling shell commands in a single line, without using the system() function, instead fork(), exec(), wait() functions are used. "|", ">", "<" operators are implemented. |
| [Concurrent File Access System 1](https://github.com/burraaook/systems-programming/tree/main/03_concurrent-file-access-system-process) | Process Synchronization, Process Communication, Shared Memory, Semaphores | Implementing a concurrent file access system. |
| [Concurrent File Access System 2](https://github.com/burraaook/systems-programming/tree/main/04_concurrent-file-access-system-thread) | Pthread Library, Thread Creation, Thread Termination, Thread Synchronization, Thread Communication, Mutexes, Condition Variables | Implementing a concurrent file access system using threads. |
| [pCp](https://github.com/burraaook/systems-programming/tree/main/05_pcp) | Thread Synchronization | Implementing a directory copying utility "pCp" using threads. |
| [BibakBOX](https://github.com/burraaook/systems-programming/tree/main/06_bibakbox) | Socket Programming, Thread Synchronization | Implementing a similar version of dropbox using sockets and threads. |
