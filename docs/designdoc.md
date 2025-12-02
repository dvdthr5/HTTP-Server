# **Design Document for HTTP Server Final Project**
## Authors: **David Glover, Nikhil Binu, Koa Wolfe**
___

### Modularization Plan:

The current plan is to split the code into 8 different files, each with a clear goal, that will hide the internals, and output only what other modules will require to carry out their specfic functions. Current file plans are main, server, http, connection, dispatcher, file, log, and parallel. 

#### main.c: 

This will be the file that sets up the program and parses the arguments. It's roles will be to validate the port number and determine whether the server needs to be parallelzed. If it does this is where we will initialize the threadpool. This will also be where we initialize the server and keep it running until it is requested to be closed. 

#### server.c: 

In this file we will create the listening socket, bind to it, and begin to listen in a loop waiting for connections. In this file we accept connections and enqueue the requests into our thread pool. This will be our networking module.

#### http.c:

In this file we will parse the http request line and headers. This is also where we will handle all errors in an http request, such as malformed line, inaccessible files etc. We won't preform any complex operations in this file other than parsing the input into its required parts. 

#### connection.c: 

We will use this file to handle one connection from end to end. It will receive the file descripor for the client and use it to read from the TCP socket into a buffer. We will then pass that buffer to http.c to parse the request. We will determine whether this is a GET or a PUT request and pass that onto dispatcher.c. We will also call log.c to record the log of this connection. After the connection is complete, we will close the socket.

#### dispatcher.c:

This is the file where we will implement the GET/PUT semantics. With each specified file we will initiate and hold a lock to ensure no concurrency violations once we parallelize. For GET requests we will check that the file path exists, check our permissions, and if accessible, read the file. For PUT requests we will create and write to the file if it doesn't exist, and overwrite the file contents if it does. We can use a shared hashmap in this file to keep track of all the mutexs, if we make the filepath/name the key, we can simply check if the lock is being held, and if so block the thread trying to access it until the lock is released. This is how we will prevent data races when writing to files. 

#### file.c: 

We will use this file to securley open files with the specified flags, read the file contents into memory, and write the contents to the disk. This file will also have to detect whether or not the file exists and return error codes related to errors in the file specification

#### log.c:

We will use this file to produce the required audit logs to stderr. It will format the audit logs, write them to stderr. In this file we will implement a global lock that will prevent data races in the audit log and ensure that they are printed in the order they finished in. 

#### parallel.c: 

We will use this file only when the -p flag is specified, and will implement the system to concurrently call the required functions or methods. This is where we will implement our thread pool that creates the specified number of workers which listen until a connection is made. Using a threadpool will save us the overhead of creating a new thread for each request and ensure that files are accessed in the order they are requested. 

---

### Abstraction Plan:


---

### Parallelization Plan: 


--- 

### Local Test Plan:

We will use the built in GitLabs CI pipeline to automate compilation and testing. This way we can define the tests that each file will need to pass, before even writing any server code. Since we have already planned our modularization and abstractions, we can write tests to ensure that the build completes and that each file correctly outputs its intended result before the entire system is complete. 

I have created a test-config.txt file that we will use to keep track of which modules are a work in progress and which ones are finished in order to be able to continuously test as we go. WIP modules will have their corresponding test cases skipped, since they aren't properly implemented and the complete modules makred as DONE will run their allotted tests. 

The CI/CD pipeline method of testing will be the most optimal way for us to locally check, since it will run the testing scripts automatically as we are working, letting us know if what we think is a complete module is functioning how we expect it, without having to manually run the tests. 

---


