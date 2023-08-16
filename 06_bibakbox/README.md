# BibakBOX
- It is similar to Dropbox. Directories on the server, and clients are synchronized.

Compilation
-------------------
```
make
```
  
RUN Server
-------------------
```
./BibakBOXServer <directory> <threadPoolSize> <portNumber>
```  

RUN Client
-------------------
```
./BibakBOXClient <directory> <portNumber> <serverIP (optional)>
```

# Some implementation details
- Communication between client and server is done via stream sockets.
- Threads are synchronized using mutexes, and conditional variables.
- Read/Write mutex is used for simultaneous read/write operations between server threads.
- It could also work on different machines.
- Signals are handled.
- No memory leaks.
- Other details are in the doc.pdf file.