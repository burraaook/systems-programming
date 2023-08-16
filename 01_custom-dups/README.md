# General  

## Part I
- It reads number of bytes from command line arguments. Then writes it using lseek before each write, two process run same time and writes to same file, results compared.  

## Part II
- Implementing dup, and dup2 system calls using fcntl.  

Compilation
-------------------
```
make
```
  
RUN Part I
-------------------
```
./appendMeMore f1 <number_of_bytes> & ./appendMeMore f1 <number_of_bytes>
```
  
```
./appendMeMore f2 1000000 x & ./appendMeMore f2 1000000 x
```
  

RUN Part II
-------------------
```
./dupsTest
```  

```
./dupsTest file1 file2
```