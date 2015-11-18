To build, run the following:

$ make clean
$ make

This will produce an executable named "sfs" in the current directory, which allows for mounting of the filesystem

$ ./sfs /tmp/mymountpoint -d

To test the code, run

$ make test1

This will create a file named "sfs_test1" in the current directory. Run the test by running the file

$ ./sfs_test1

similarly for running test2 execute

$ make test2
$ ./sfs_test2
