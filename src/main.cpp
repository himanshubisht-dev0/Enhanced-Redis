#include<iostream>
#include "../include/RedisServer.h"
int main(int argc,char* argv[]){
   int port =6379;//default port
   if(argc>=2)port=std::stoi(argv[1]);//checking if server wants the user wants to start server or not.if not we use default.
   
   RedisServer server(port);

    return 0;
}