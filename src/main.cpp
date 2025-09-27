#include<iostream>
#include<thread>
#include<chrono>
#include "../include/RedisServer.h"
#include "../include/RedisDatabase.h"
int main(int argc,char* argv[]){
   int port =6379;//default port
   if(argc>=2)port=std::stoi(argv[1]);//checking if server wants the user wants to start server or not.if not we use default.
   
   RedisServer server(port);

   //background persistance: dump the database every 300 seconds.(5*60 save database)
   std::thread persistanceThread([](){
    while(true){
        std::this_thread::sleep_for(std::chrono::seconds(300));
        //dump the database 
        if(!RedisDatabase::getInstance().dump("dump.my_rdb")){
            std::cerr<<"Error dumping database\n";
        }else {
            std::cout<<"Database dumped successfully\n";
        }

    }
   });
   persistanceThread.detach();

   server.run();
    return 0;
}