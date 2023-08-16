# Concurrent File Access System Using Threads
- A file Server that enables multiple clients to connect, access and modify the contents of files in a specific directory.

Compilation
-------------------
```
make
```
  
RUN Server
-------------------
```
./biboServer <dirname> <max #ofClients>
```  

RUN Client
-------------------
```
./biboClient <Connect, tryConnect> ServerPID
```

# Some implementation details
- Communication between client and server is done via FIFOs.  
- Clients are held in queue if max #ofClients is reached.
- Shared memory is used for download, and upload operations.  
- Threads are synchronized using mutexes, and conditional variables.  
- Read/Write mutex is used for simultaneous read/write operations.  
- Other details are in the doc.pdf file.