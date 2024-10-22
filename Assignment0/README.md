# Assignment 0: Socket Programming


**Please make sure to thoroughly read this before getting to the implementation.**

Socket programming is the standard way to write programs that communicate over a network. While originally developed for Unix computers programmed in C, the socket abstraction is general and not tied to any specific operating system or programming language. This allows programmers to use socket to write correct network programs in many contexts. This assignment will give you experience with basic socket programming. **Using C**, you will write one pair of TCP client and server program for sending and receiving text messages over the Internet. The client and server programs should meet the following specifications. 

## Server Specifications

* Each server program should listen on a socket, wait for a client to connect, receive a message from the client, print the message to stdout, and then wait for the next client indefinitely.

* Each server should take one command-line argument: the port number to listen on for client connections.
* Each server should accept and process client communications in an infinite loop, allowing multiple clients to send messages to the same server. The server should only exit in response to an external signal (e.g. SIGINT from pressing ctrl-c).

* Each server should maintain a short (10) client queue and handle multiple client connection attempts sequentially. **In real applications, a TCP server would fork a new process to handle each client connection concurrently, but that is NOT necessary for this assignment.**

* Each server should gracefully handle error values potentially returned by socket programming library functions. Errors related to handling client connections should not cause the server to exit after handling the error; all others should.

* Each server should be able to handle very large messages in both alphanumeric and binary format.

## Client Specifications

* Each client program should contact a server, read a message from stdin, send the message, and exit.
* Each client should read and send the message exactly as it appears in stdin until reaching an EOF (end-of-file).

* Each client should take two command-line arguments: the IP address of the server and the port number of the server.

* Each client must be able to handle arbitrarily large messages by iteratively reading and sending chunks of the message, rather than reading the whole message into memory first.

* Each client should handle partial sends (when a socket only transmits part of the data given in the last send call) by attempting to re-send the rest of the data until it has all been sent.

* Each client should gracefully handle error values potentially returned by socket programming library functions.

## Getting Started

The classic "Beej's Guide to Network Programming" is located here: https://beej.us/guide/bgnet/html/.  The [system call section](https://beej.us/guide/bgnet/html/#system-calls-or-bust) and [client/server example section](https://beej.us/guide/bgnet/html/#client-server-background) will be most relevant. The man pages are also useful for looking up individual functions (e.g. `man socket`).

We have provided scaffolding code in the **Assignment0/Starter_Codes** directory. The files `client.c` and `server.c` contain the scaffolding code. You will need to add socket programming and I/O code. You should read and understand this code before starting to program. For error handling, you can call `perror` for socket programming functions that set the global variable `errno` (Beej's Guide will tell you which do). For those that don't, simply print a message to standard error. Use GCC to compile your code, for example: `gcc server.c -o server`. The server should be run as `./server [port] > [outputfile]`. The client should be run as `./client [server IP] [server port] <[message file]`.

**Note: DO NOT change the file names, as they will be used for automated testing.**

## Testing your code

You should test your implementations by attempting to send messages from your clients to your servers. The server can be run in the background (append a `&` to the command) or in a separate SSH window. You should use `127.0.0.1` as the server IP and a high server port number between 10000 and 60000. You can kill a background server with the command `fg` to bring it to the foreground then `ctrl-c`.

Make sure that your client and server support the following:

* Any size of alphanumeric messages

* Any size of binary messages

* Several messages sent sequentially from separate clients to one server

* Several messages sent concurrently from separate clients to one server

Also note that we will be using IPv4 (127.0.0.1) to run the server process. When defining 'sockaddr_in' struct, make sure you provide flags that bind the socket to IPv4 (AF_INET) or both IPv4 and IPv6 (AF_UNSPEC).

## Test Environment & Debugging
Here are some debugging tips. If you are still having trouble, ask a question on Canvas or see an instructor during office hours.

* Different OSes might slightly change in the way they implement the systemcalls. As Gradescope uses Ubuntu >= 20.04, make sure your code also works and covers all the requirements on Ubuntu. For this purpose, you can follow the instruction on the assignments repository README file to quickly set-up an Ubuntu virtual machine on your Laptop/PC. But in general, you may use any of these systems for this assignment:
    1. Any system with a recent Linux OS installed
    2. Any MAC system (Intel/M1/M2)
    3. Windows Subsystem for Linux (WSL)
    4. CS Undergrad/Masters servers
    5. The provided Multipass VM environment (see the root repository README file)

* There are defined buffer size and queue length constants in the scaffolding code. Use them. If they are not defined in a particular file, you don't need them. If you are not using one of them, either you have hard-coded a value, which is bad style, or you are very likely doing something wrong.

* There are multiple ways to read and write from stdin/stdout in C. If you'r code seems correct but you are failing the tests, make sure the problems isn't caused by how you read and write. We suggest to use `printf`. Also, make sure to flush the stdout by calling `fflush(stdout)`.

* If you are using buffered I/O to write to stdout, make sure to call flush or the end of a long message may not write.

* Remember to close the socket at the end of the client program.
    
* When testing, make sure you are using 127.0.0.1 as the server IP argument to the client and the same server port for both client and server programs.

* If you are getting connection errors, try a different port between 10000 and 60000.


## Submission

Please submit your `server.c` and `client.c` files to Gradescope. You may include a README if you feel that there is anything that you need to explain to the graders, but this is not required. Please do not submit any additional files or executables.

Good luck!