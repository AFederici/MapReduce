# MP2 Simple Distributed File System

## Executing Instructions
 * Building Node
```
$ make all
```

 * We are running All-to-All in MP2 (Note: Gossip-style is not tested)
```
$ ./Node 0
```

 * Running time commands
```
$ [join] join to a group via fixed introducer
$ [leave] leave the group
$ [id] print id (IP/PORT)
$ [member] print all membership list
$ [switch] switch to other mode (All-to-All to Gossip, and vice versa)
$ [mode] show in 0/1 [All-to-All/Gossip] modes
$ [exit] terminate process
$ === New since MP2 === 
$ [put] localfilename sdfsfilename
$ [get] sdfsfilename localfilename
$ [delete] sdfsfilename
$ [ls] list all machine (VM) addresses where this file is currently being stored
$ [lsall] list all sdfsfilenames with positions
$ [store] list all files currently being stored at this machine
```

 * Create files
```
$ dd if=/dev/urandom of=test_file bs=2097152 count=2
$ dd if=/dev/urandom of=test_file_07 bs=1000000 count=7
```


 * All logs are in `logs.txt` under the mp2 folder 

## Acknowledgement
 * [Beej's guide](http://beej.us/guide/bgnet/html/multi/index.html)
 * [Multiple Threads](https://www.tutorialspoint.com/cplusplus/cpp_multithreading.htm)
 * [String parser](https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c)
 * [Wikicorpus](https://www.cs.upc.edu/~nlp/wikicorpus/)