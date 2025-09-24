/*
    1.command-Line argument parsing
    -h<host> default :127.0.0.1 -p<port> default : 6379
    if no args,launch interaction REPL mode
2. Object oriented programming
   RedisClient, CommondHandler(parsing input), ResponseParser,CLI(I/O)

3. Establish TCP connection to redis(RedisClient)
    Berkeley sockets to open TCP connection
    IPv4 and IPv6 'getaddrinfo'
    Implements:
        connectToServer()->Establishes the connection.
        sendCommand()->Sends a command over the socket.
        disconnect()->closes the socket when finished.
4. Parsing and command fomatting <commandHandler>
    split user input 
    Convert commands into RESP format:
    ...
    *3\r\n
    $3\r\nSET\r\n
    $3\r\nkey\r\n
    $5\r\nvalue\r\n
    ...
5. Handling Redis Responses(RedisParser)
    Read servr responses and parse RESP types
    +OK,-Error,:100,$5\r\nhello->Bulk string, *2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n->Array response
6. Interactive REPL mode(CLI)
 Run loop: Input, valis redis  commands, send commands to the server, display parsed response
 Support:help,quit
7.  main.cpp parse command-line args, instantiate CLI and launch in REPL mode.

Socket programming
Protocal programming
OOP principles
CLI development
*/
#include "CLI.h"
#include<string>
#include <iostream>
int main(int argc, char* argv[]){
    std::string host="127.0.0.1";
    int port=6379;
    int i=1;
    std::vector<std::string> commandArgs;
    //parse command line args for -h and -p
    while(i<argc){
        std::string arg=argv[i];
        if(arg=="-h" && i+1<argc){//-h 127.0.0.1 
            host=argv[++i];

        }else if(arg=="-p" && i+1<argc){
            port=std::stoi(argv[++i]);
        }else{
            //Remaining args
            while(i<argc){
                commandArgs.push_back(argv[i]);
                i++;
            }
            break;
        }
        ++i;
    }
    //Handle REPL and one-shot command mode
    CLI cli(host,port);
    cli.run(commandArgs);

    return 0;
}