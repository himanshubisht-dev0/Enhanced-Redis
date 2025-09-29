#include<iostream>//debug
#include"../include/RedisCommandHandler.h"
#include "../include/RedisDatabase.h"
#include<vector>
#include<sstream>
#include<algorithm>
//RESP parser:
//*2\r\n$4\r\n\PING\r\n$4\r\nTest\r\n
//*2->array has 2 elements
//$4-> next string as 4 characters
//PING 
//TEST

std::vector<std::string> parseRespCommand(const std::string &input){
    std:: vector<std::string>tokens;
    if(input.empty())return tokens;
    //if it doesn't start with '*' ,fallback to splitting by whitespace.
    if(input[0]!='*'){
        std:: istringstream iss(input);
        std::string token;
        while(iss>>token){
            tokens.push_back(token);
        }
        return tokens;
    }
    size_t pos=0;
    //Expect '*' followed by number of elements
    if(input[pos]!='*')return tokens;
    pos++;//skip '*'
    //crlf * carriage Return (\r) , line feed(\n)
    size_t crlf=input.find("\r\n",pos);
    if(crlf==std::string::npos) return tokens;

    int numElements = std::stoi(input.substr(pos,crlf-pos));// we will check input the subset is from the position to length
    pos=crlf+2;//skip 2 characters
    
    for(int i=0;i<numElements;i++){
        if((pos>=input.size()) || input[pos]!='$')break;//format error
        pos++;//skip'$'

        crlf=input.find("\r\n",pos);
        if(crlf==std::string::npos)break;
        int len=std::stoi(input.substr(pos,crlf-pos));
        pos=crlf+2;
        if(pos+len>input.size())break;
        std::string token=input.substr(pos,len);
        tokens.push_back(token);
        pos+=len+2;//skip token and crlf


    }
    return tokens;

}
RedisCommandHandler::RedisCommandHandler(){}

std::string RedisCommandHandler::processCommand(const std::string& commandLine){
    //use RESP protocol
    auto tokens=parseRespCommand(commandLine);
    if(tokens.empty())return "-Error:Empty command\r\n";

    // std::cout<<commandLine<<"\n";// Hello world -> *2 $5 Hello $5 world Hello world 

    // for(auto& t:tokens){
    //     std::cout<<t<<"\n";
        
    // }only for debug
    std::string cmd=tokens[0];
    std:: transform(cmd.begin(),cmd.end(),cmd.begin(),::toupper);
    std::ostringstream response;
    
    RedisDatabase& db = RedisDatabase::getInstance();


    //connect to database
    //check commands
    //Common commands
    if(cmd=="PING"){
        response<<"+PONG\r\n";

    }else if(cmd=="ECHO"){
        if(tokens.size() < 2) response << "-Error: Echo requires a message\r\n";
        else{
            response << "+" << tokens[1]<<"\r\n";
        }

    }
    else if (cmd=="FLUSHALL"){
        db.flushAll();
        response<<"+OK\r\n";
    }
    //Key/value operations
    else if(cmd=="SET"){
        if(tokens.size()<3){
            response<<"-Error: SET required key and value\r\n";
        }else {
            db.set(tokens[1],tokens[2]);
            response<<"+OK\r\n";
        }
    }
    else if(cmd=="GET"){
        if(tokens.size()<2){
            response<<"-Error: Get requires key\r\n";
        }
        else{
            std::string value;
            if(db.get(tokens[1],value)){
                response<<"$"<<value.size()<<"\r\n"<<value<<"\r\n";
            }else{
                response<<"$-1\r\n";
            }
        }
    }
        //Key/Value Operations

    else if(cmd=="KEYS"){
        std::vector<std::string>allkeys=db.keys();
        response<<"*"<<allkeys.size()<<"\r\n";
        for(const auto& key: allkeys){
            response<<"$"<<key.size()<<"\r\n"<<key<<"\r\n";
        }
    }
    else if(cmd=="TYPE"){
        if(tokens.size()<2){
            response<<"-Error: TYPE requires key\r\n";
        }else{
            response <<"+"<<db.type(tokens[1])<<"\r\n";
        }
    }
    else if(cmd=="DEL" || cmd=="UNLINK"){
        if(tokens.size()<2)
            response <<"-Error: "<<cmd<<" requires key\r\n";
        else {
            bool res=db.del(tokens[1]);
            response << ":" << (res?1:0)<<"\r\n";
        }
    }
    else if(cmd=="EXPIRE"){
        if(tokens.size()<3){
            response<< "-Error: EXPIRE requires key and time in seconds\r\n";
        }
        else{
            int seconds=std::stoi(tokens[2]);
            if(db.expire(tokens[1],seconds))
                response<<"+OK\r\n";
        }
    }
    else if(cmd=="RENAME"){
        if(tokens.size()<3){
            response<<"-Error : RENAME requires new key name and old key name\r\n";
        }else{
            if(db.rename(tokens[1],tokens[2]))
                response<<"+OK\r\n";
        }
    }
    //List operations
    //Hash Operations

    else{
        response<<"-ERROR: Unkown command\r\n";
    }

    return response.str();
}