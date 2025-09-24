#include "CLI.h"
#include<vector>

//simple helper to trim whitespace 
static std::string trim(const std::string &s){
    size_t start=s.find_first_not_of(" \t\n\r\f\v");
    if(start==std::string::npos) return "";
    size_t end=s.find_last_not_of(" \t\n\r\f\v");
    return s.substr(start,end-start+1); 
}
CLI::CLI(const std:: string &host,int port)
    :host(host),port(port),redisClient(host,port){}

void CLI::run(const std::vector<std::string>& commandArgs){
    if(!redisClient.connectToServer()){
        return;
    }
    if(!commandArgs.empty()){
        executeCommand(commandArgs);
    }
    std::cout<<"Connected to Redis at "<<redisClient.getSocketFD()<<"\n";
 
    while(true){
        std::cout<<host<<" : "<<port<<"> ";
        std::cout.flush();//clear the output buffer
        std::string line;
        if(!std::getline(std::cin,line))break;
        line=trim(line);
        if(line.empty())continue;
        if(line=="quit" || line=="exit"){
            std::cout<<"Goodbye!\n";
            break;
        }
        if(line=="help"){
            std::cout<<"Displaying help\n";
            continue;
        }
        //split commands into tokens
        std::vector<std::string>args=CommandHandler::splitArgs(line);
        if(args.empty())continue;

        // for(const auto &arg:args){
        //     std::cout<< arg<<"\n";
        // }
        std::string command= CommandHandler::buildRESPcommand(args);
        if(!redisClient.sendCommand(command)){
            std::cerr<<"(Error) failed to send command.\n";
            break;
        }
        //Parse and print response 
        std::string response=ResponseParser::parserResponse(redisClient.getSocketFD());
        std::cout<< response<< "\n";

    }
    redisClient.disconnect();
}
void CLI::executeCommand(const std::vector<std::string> &args){
    if(args.empty())return;

    std::string command=CommandHandler::buildRESPcommand(args);
   
    if(!redisClient.sendCommand(command)){
        std::cerr<<"(Error) failed to send command.\n";
        return;
    }
    //Parse and print response
    std::string response=ResponseParser::parserResponse(redisClient.getSocketFD());
    std::cout<<response<<"\n";
}