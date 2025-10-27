#include "D:\\projects\\Enhanced-Redis\\include\\RedisServer.h"
#include "D:\\projects\\Enhanced-Redis\\include\\RedisCommandHandler.h"
#include "D:\\projects\\Enhanced-Redis\\include\\RedisDatabase.h"
#include "D:\\projects\\Enhanced-Redis\\include\\ThreadPool.h"

#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <thread>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <vector>
#include <signal.h> // For signal handling
#include <atomic>   // For std::atomic

static RedisServer *globalServer = nullptr;

void signalHandler(int signum)
{
    if (globalServer)
    {
        std::cout << "Caught signal " << signum << " ,shutting down...\n";
        globalServer->shutdown();
    }
    // Exit here, as the server's main loop will terminate after `running` becomes false
    // and `accept` returns an error, or the loop naturally ends.
    // If globalServer is nullptr or shutdown() doesn't immediately exit, this ensures termination.
    exit(signum); 
}

void RedisServer::setupSignalHandler()
{
    // Register signal handler for SIGINT (Ctrl+C)
    signal(SIGINT, signalHandler); 
}

RedisServer::RedisServer(int port) : 
    port(port), 
    server_socket(-1), 
    running(true), 
    thread_pool(std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4) // Initialize thread pool
{
    globalServer = this; // Set global pointer for signal handling
    setupSignalHandler(); // Setup signal handler
}

void RedisServer::shutdown()
{
    running = false; // Atomically set running flag to false

    // Attempt to persist the database before closing the socket.
    // This should ideally be done only once during a graceful shutdown.
    if(RedisDatabase::getInstance().dump("dump.my_rdb")){
        std::cout << "Database dumped to dump.my_rdb\n";
    } else {
        std::cerr << "Error dumping database\n";
    }

    if (server_socket != -1)
    {
        // Close the server socket to unblock the accept() call in the run loop.
        // This will cause accept() to return -1, and the loop will check `running`.
        close(server_socket);
    }
    std::cout << "Server shutdown complete\n";
    // The thread_pool destructor will implicitly join all threads when RedisServer goes out of scope.
}

void RedisServer::run()
{
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        std::cerr << "Error creating server socket\n";
        return;
    }

    // Set socket options for reuse address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "Error setting socket options\n";
        close(server_socket);
        return;
    }

    // Bind socket to address and port
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY; // Listen on all available interfaces

    if (bind(server_socket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        std::cerr << "Error binding server socket\n";
        close(server_socket);
        return;
    }

    // Listen for incoming connections
    if (listen(server_socket, 10) < 0) // Max 10 pending connections
    {
        std::cerr << "Error listening on server socket\n";
        close(server_socket);
        return;
    }
    std::cout << "SmartCacheDB Listening on Port " << port << "\n";

    // Load database on startup if dump file exists
    if(RedisDatabase::getInstance().load("dump.my_rdb")){
        std::cout << "Database loaded from dump.my_rdb\n";
    } else {
        std::cout << "No dump found or load failed; starting with an empty database.\n";
    }

    // Create a single command handler instance, potentially shared by threads if it's thread-safe
    // If RedisCommandHandler itself needs per-thread state, it should be created per-thread or passed by value.
    // Given its current structure (processing a single command and returning a string),
    // passing it by reference to the enqueued task is fine, assuming its internal state is managed via RedisDatabase mutex.
    RedisCommandHandler cmdHandler; 

    while (running) // Loop as long as the server is running
    {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        // Accept a new client connection
        int client_socket = accept(server_socket, (struct sockaddr *)&clientAddr, &clientLen);
        
        if (client_socket < 0)
        {
            if (running) // If server is still supposed to be running, it's an error
            {
                std::cerr << "Error accepting client connection\n";
            }
            // If `running` is false, `accept` failed because `server_socket` was closed.
            // In this case, it's not an error, just part of shutdown.
            break; 
        }

        // Enqueue the client handling task to the thread pool
        // The lambda captures client_socket by value to ensure each task has its own descriptor.
        // cmdHandler is captured by reference, assuming it's thread-safe or its critical sections are guarded by RedisDatabase's mutex.
        thread_pool.enqueue([client_socket, &cmdHandler]() 
        {
            char buffer[1024]; // Buffer for receiving client data
            while(true) // Loop to continuously receive commands from this client
            {
                memset(buffer, 0, sizeof(buffer)); // Clear buffer
                // Receive data from client
                int bytes = recv(client_socket, buffer, sizeof(buffer)-1, 0);
                if(bytes <= 0) break; // Client disconnected or an error occurred
                
                std::string request(buffer, bytes); // Construct request string
                std::string response = cmdHandler.processCommand(request); // Process command
                send(client_socket, response.c_str(), response.size(), 0); // Send response back to client
            }
            close(client_socket); // Close client socket when done
        });
    }
    // The ThreadPool's destructor will automatically join all worker threads when the RedisServer object
    // is destroyed at the end of its scope (or when `main` exits and globalServer is cleaned up).
}