#Running Executables on macOS from Memory

This is the code repo for the blog post located <here>.

##symbolyzer.py

symbolyzer.py is used for generating unique offsets / ints for a list of symbols to use for matching in place of a hash algorithm.  It can be run in the following manner to do so for dyld: 

> $ nm /usr/lib/dyld | cut -d" " -f3 | sort | uniq | python symbolyzer.py

The output is in the following format: 

> $ _sysctlbyname[4] = 0x626c7463

This means that if you read an int from the start of the string table entry + 4 and get 0x626c7463, you have found the entry for sysctlbyname.

##run_bin.c

run_bin.c is the proof-of-concept code for running a binary from memory.  It can be run as follows:

> $ gcc run_bin.c && ./a.out /bin/ls
