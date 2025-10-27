#ifndef REDIS_SERVER_H
#define REDIS_SERVER_H

#include<string>//signal handling
#include<atomic>
#include "D:\\projects\\Enhanced-Redis\\include\\ThreadPool.h" // Include ThreadPool header

class RedisServer{
public:
    RedisServer(int port);
    void run();
    void shutdown();
private:
    int port;
    int server_socket;
    std::atomic<bool> running;
    ThreadPool thread_pool; // Add a ThreadPool member

    //Setup signal handlers for graceful shutdown (crtl +c)
    void setupSignalHandler();

};
#endif