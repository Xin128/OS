compile as:
gcc -o spooler spooler.c -lrt -pthread
gcc -o client client.c -lrt -pthread

* spooler parameters should be input as
  ./spooler [queue_size]
  where queue_size is an optional argument
  otherwise queue_size defaults to 10

* client parameters should be input as
 ./client clientId pages
 clientId can be any alpha-numeric sequence of 3 characters, max
 pages must be a positive integer
 all other arguments will be ignored
