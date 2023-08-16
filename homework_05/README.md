# Directory Copying Utility
- A directory copying utility works like "cp -r" command.  

Compilation
-------------------
```
make
```
  
RUN
-------------------
```
./main <buffer_size> <number_of_consumers> <source_dir> <dest_dir>
```  

# Some implementation details
- Producer-Consumer model is used.
- Read/Write mutex is used for simultaneous read/write operations.
- Maximum open file descriptor is considered.  
- Other details are in the doc.pdf file.  